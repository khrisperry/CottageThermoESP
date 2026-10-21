import copy
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import importlib.util
import io
import json
from pathlib import Path
import sys
import threading
import time
import unittest
import urllib.error
import urllib.request

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'ha_bridge'))
from bridge import Bridge, BridgeError, HAClient, Server, Settings
from PIL import Image

MAPPING=json.loads((ROOT/'ha_bridge/entities.json').read_text())['entities']
TOKEN='b'*32

class FakeHA:
    def __init__(self):
        self.calls=[]
        self.fail=None
        self.missing=set()
        self.raw={}
        services={'light':['turn_on','turn_off'],'input_boolean':['turn_on','turn_off'],
            'cover':['open_cover','stop_cover','close_cover'],'vacuum':['start','stop']}
        self.services=[{'domain':d,'services':{s:{} for s in names}} for d,names in services.items()]
        for key,spec in MAPPING.items():
            state='idle' if spec['kind'] in ('laundry','vacuum','camera') else 'off'
            attrs={}
            if spec['kind']=='temperature':state='18.5';attrs={'unit_of_measurement':'°C'}
            if spec['kind']=='cover':state='open';attrs={'supported_features':11,'current_position':60}
            if spec['kind']=='vacuum':attrs={'supported_features':8200}
            if spec['kind']=='camera':attrs={'access_token':'DO_NOT_EXPOSE','entity_picture':'/api/camera_proxy/x?token=DO_NOT_EXPOSE'}
            self.raw[spec['entity_id']]={'entity_id':spec['entity_id'],'state':state,'attributes':attrs}
    def request(self,path,payload=None,binary=False):
        self.calls.append((path,copy.deepcopy(payload)))
        if self.fail is not None:raise BridgeError(self.fail,'Test upstream failure')
        if path=='/api/services':return copy.deepcopy(self.services)
        if payload is not None:return []
        if binary:
            b=io.BytesIO();Image.new('RGB',(1280,720),'blue').save(b,'JPEG');return b.getvalue()
        entity=path.rsplit('/',1)[-1]
        if entity in self.missing:raise BridgeError(404,'Missing entity')
        return copy.deepcopy(self.raw[entity])

class BridgeTests(unittest.TestCase):
    def setUp(self):
        self.now=100
        self.fake=FakeHA()
        self.settings=Settings('https://test.invalid','SECRET_HA_TOKEN',TOKEN,copy.deepcopy(MAPPING))
        self.bridge=Bridge(self.settings,self.fake,clock=lambda:self.now)
        self.bridge.poll_once()
    def item(self,key):return next(i for i in self.bridge.snapshot()['items'] if i['key']==key)
    def test_exact_mapping_and_minimal_snapshot(self):
        self.assertEqual(len(self.bridge.snapshot()['items']),11)
        self.assertEqual(len([i for i in self.bridge.snapshot()['items'] if i['kind']=='laundry']),4)
        self.assertNotIn('DO_NOT_EXPOSE',json.dumps(self.bridge.snapshot()))
        self.assertNotIn('SECRET_HA_TOKEN',json.dumps(self.bridge.snapshot()))
        self.assertEqual(self.item('kids_temp')['unit'],'°C')
        self.assertEqual(self.item('kids_temp')['value'],18.5)
        self.assertNotIn('remaining',self.item('washer1'))
    def test_all_requested_control_routes(self):
        for key,action,path in [('deck','on','light/turn_on'),('deck','off','light/turn_off'),
          ('breezeway','on','light/turn_on'),('kids_heat','off','input_boolean/turn_off'),
          ('blinds','open','cover/open_cover'),('blinds','close','cover/close_cover'),
          ('blinds','stop','cover/stop_cover'),('vacuum','start','vacuum/start'),('vacuum','stop','vacuum/stop')]:
            result=self.bridge.command({'key':key,'action':action,'request_id':key+action+'-unique'})
            self.assertTrue(result['accepted'])
            self.assertIn(('/api/services/'+path,{'entity_id':MAPPING[key]['entity_id']}),self.fake.calls)
        self.assertFalse(any('climate/' in p or '/api/states/' in p and data for p,data in self.fake.calls))
    def test_no_arbitrary_entity_action_or_furnace_controls(self):
        for payload in [{'key':'kids_temp','action':'on','request_id':'12345678'},
           {'key':'furnace','action':'on','request_id':'12345678'},
           {'key':'deck','action':'toggle','request_id':'12345678'},
           {'key':'deck','action':'on','request_id':'12345678','entity_id':'switch.other'}]:
            with self.assertRaises(BridgeError):self.bridge.command(payload)
    def test_ambiguous_post_not_repeated(self):
        self.fake.fail=503
        cmd={'key':'deck','action':'on','request_id':'12345678'}
        self.assertEqual(self.bridge.command(cmd)['outcome'],'unknown')
        before=len(self.fake.calls)
        self.assertEqual(self.bridge.command(cmd)['outcome'],'unknown')
        self.assertEqual(len(self.fake.calls),before)
        with self.assertRaises(BridgeError):self.bridge.command({**cmd,'action':'off'})
    def test_success_is_idempotent_but_observed_state_wins(self):
        cmd={'key':'deck','action':'on','request_id':'success-123'}
        self.assertTrue(self.bridge.command(cmd)['accepted'])
        before=len(self.fake.calls);self.bridge.command(cmd)
        self.assertEqual(len(self.fake.calls),before)
        self.assertEqual(self.item('deck')['state'],'off') # HA has not reported on; no optimistic success state.
    def test_outage_and_ttl_disable_commands(self):
        self.fake.fail=503;self.bridge.poll_once()
        self.assertTrue(all(not i['available'] for i in self.bridge.snapshot()['items']))
        self.fake.fail=None;self.bridge.poll_once();self.assertTrue(self.item('deck')['available'])
        self.now+=31
        self.assertFalse(self.item('deck')['available'])
        self.assertEqual(self.item('deck')['actions'],[])
        self.assertNotIn('value',self.item('kids_temp'))
    def test_missing_entity_does_not_remove_others(self):
        self.fake.missing.add(MAPPING['washer2']['entity_id']);self.bridge.poll_once()
        self.assertFalse(self.item('washer2')['available'])
        self.assertTrue(self.item('washer1')['available'])
        self.assertTrue(self.item('vacuum')['available'])
    def test_features_and_nonfinite_temperature(self):
        self.fake.raw[MAPPING['vacuum']['entity_id']]['attributes']['supported_features']=8
        self.fake.raw[MAPPING['kids_temp']['entity_id']]['state']='nan'
        self.bridge.poll_once()
        self.assertEqual(self.item('vacuum')['actions'],['stop'])
        self.assertFalse(self.item('kids_temp')['available'])
    def test_camera_on_demand_bounded_and_cached(self):
        self.assertFalse(any('/api/camera_proxy/' in p for p,_ in self.fake.calls))
        frame=self.bridge.frame();calls=len(self.fake.calls)
        self.assertLessEqual(len(frame),96000)
        with Image.open(io.BytesIO(frame)) as image:self.assertEqual(image.size,(480,270))
        self.assertEqual(self.bridge.frame(),frame);self.assertEqual(len(self.fake.calls),calls)
        self.now+=2;self.bridge.frame();self.assertEqual(len(self.fake.calls),calls+1)
    def test_token_missing_sends_no_requests(self):
        fake=FakeHA();bridge=Bridge(Settings('https://test.invalid','',TOKEN,MAPPING),fake)
        bridge.poll_once();self.assertFalse(fake.calls);self.assertFalse(bridge.snapshot()['configured'])

class HTTPTests(unittest.TestCase):
    def setUp(self):
        self.fake=FakeHA();self.bridge=Bridge(Settings('https://test.invalid','SECRET',TOKEN,MAPPING),self.fake)
        self.bridge.poll_once()
        self.server=Server(('127.0.0.1',0),self.bridge)
        self.thread=threading.Thread(target=self.server.serve_forever,daemon=True);self.thread.start()
        self.url='http://127.0.0.1:'+str(self.server.server_port)
    def tearDown(self):self.server.shutdown();self.server.server_close();self.thread.join()
    def get(self,path,token=TOKEN,data=None):
        return urllib.request.urlopen(urllib.request.Request(self.url+path,data=data,headers={'X-Cottage-Token':token,'Content-Type':'application/json'}),timeout=3)
    def test_auth_and_safe_html(self):
        with self.assertRaises(urllib.error.HTTPError) as e:self.get('/api/ha/status','wrong')
        self.assertEqual(e.exception.code,401)
        with self.get('/') as response:
            body=response.read().decode();self.assertNotIn('SECRET',body);self.assertNotIn(TOKEN,body)
        with self.get('/api/ha/status') as response:self.assertEqual(len(json.load(response)['items']),11)
    def test_real_http_command_body_validation_and_jpeg(self):
        with self.get('/api/ha/command',data=b'{"key":"kids_heat","action":"on","request_id":"http-1234"}') as response:
            self.assertTrue(json.load(response)['accepted'])
        with self.assertRaises(urllib.error.HTTPError) as e:self.get('/api/ha/command',data=b'{"key":"deck","key":"vacuum","action":"start","request_id":"http-1234"}')
        self.assertEqual(e.exception.code,400)
        with self.get('/api/ha/camera') as response:
            self.assertEqual(response.headers['Content-Type'],'image/jpeg');self.assertTrue(response.read().startswith(b'\xff\xd8'))
    def test_query_token_never_authorizes(self):
        with self.assertRaises(urllib.error.HTTPError) as e:self.get('/api/ha/status?token='+TOKEN,'')
        self.assertEqual(e.exception.code,401)

class TransportTests(unittest.TestCase):
    def test_verified_tls_and_no_redirect_or_retry(self):
        class Response:
            status_code=200
            def __enter__(self):return self
            def __exit__(self,*a):pass
            def iter_content(self,n):yield b'{"ok":true}'
        class Session:
            def request(self,*a,**kwargs):self.args=a;self.kwargs=kwargs;return Response()
        client=HAClient(Settings('https://example.invalid','SECRET',TOKEN,MAPPING))
        session=Session();client.local.session=session
        self.assertEqual(client.request('/api/services/light/turn_on',{'entity_id':'light.test'}),{'ok':True})
        self.assertIs(session.kwargs['verify'],True);self.assertIs(session.kwargs['allow_redirects'],False)
        self.assertEqual(session.kwargs['headers']['Authorization'],'Bearer SECRET')

if __name__=='__main__':unittest.main()
