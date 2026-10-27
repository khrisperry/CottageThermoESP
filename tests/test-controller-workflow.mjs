import fs from 'node:fs';
import assert from 'node:assert/strict';
const {Window}=await import(process.env.HAPPY_DOM_MODULE||'happy-dom');
const window=new Window({url:'http://controller.test',settings:{enableJavaScriptEvaluation:true}});
const posts=[],errors=[],intervals=[];let offline=false,delayGo=null;
const profile={start_us:600,end_us:2300,ready:true,issue:'',positions:Array.from({length:8},(_,i)=>({index:i,pulse_us:700+i*200,angle_deg:350-i*30,...(i>0&&i<7?{temp_f:45+i*5}:{})}))};
const state={operation:'stopped',simulation:true,pwm_active:false,bench_armed:false,arm_remaining_ms:0,source:'room',temperature_f:64,target_f:65,reason:'hold',encoder:{angle_valid:true,angle_deg:228.7}};
window.setInterval=fn=>{intervals.push(fn);return intervals.length;};window.confirm=()=>true;
window.addEventListener('error',e=>errors.push(e.message));
window.fetch=async(path,opts={})=>{
 if(offline)throw Error('Offline');
 if(opts.method==='POST'){
  const data=JSON.parse(opts.body);posts.push({path,data});
  if(path==='/api/stop')Object.assign(state,{operation:'stopped',simulation:true,pwm_active:false,bench_armed:false});
  else if(path==='/api/config')state.target_f=data.target_f;
  else if(path==='/api/setup'){
   if(data.action==='test')Object.assign(state,{operation:'test',simulation:true,pwm_active:false,bench_armed:true,arm_remaining_ms:900000,last_command_us:-1});
   else if(data.action==='pulse')Object.assign(state,{pwm_active:true,last_command_us:data.us});
   else if(data.action==='jog')state.last_command_us+=data.delta_us;
   else if(data.action==='go'){Object.assign(state,{operation:'go',simulation:false});if(delayGo)await delayGo;}
   else if(data.action==='temperatures')data.values.forEach((t,i)=>profile.positions[i+1].temp_f=t);
  }
  return {ok:true,json:async()=>({ok:true})};
 }
 return {ok:true,json:async()=>structuredClone(path==='/api/setup'?profile:path==='/api/events'?[]:state)};
};
const html=fs.readFileSync(new URL('../controller/main/ui.html',import.meta.url),'utf8');
const script=html.match(/<script>([\s\S]*?)<\/script>/)[1];
window.document.write(html.replace(/<script>[\s\S]*?<\/script>/,''));window.eval(script);
const id=x=>window.document.getElementById(x);
async function settle(){await new Promise(r=>setTimeout(r,25));}
await settle();assert.equal(posts.length,0,'page load must not command movement');
const ids=[...window.document.querySelectorAll('[id]')].map(x=>x.id);assert.equal(new Set(ids).size,ids.length,'element IDs must be unique');
assert(id('goButton').disabled);assert(id('sendPulse').disabled);assert(id('jogPlus').disabled);
id('testButton').click();await settle();assert.equal(posts.at(-1).data.action,'test');assert(!id('sendPulse').disabled);assert(id('jogPlus').disabled);
id('startingPulse').value='1300';id('sendPulse').click();await settle();assert.equal(posts.at(-1).data.us,1300);assert(!id('jogPlus').disabled);
id('jogStep').value='10';id('jogPlus').click();await settle();assert.equal(posts.at(-1).data.delta_us,10);assert(id('goButton').disabled,'jogged away from saved start');
id('tab-go').click();assert(!id('panel-go').hidden);assert(id('panel-calibration').hidden);
id('moveStart').click();await settle();assert.equal(posts.at(-1).data.us,profile.positions[3].pulse_us);assert(!id('goButton').disabled);
id('goButton').click();await settle();assert.equal(posts.at(-1).data.action,'go');assert.equal(posts.at(-1).data.position,3);assert.equal(state.simulation,false);assert(id('jogPlus').disabled);
id('goTarget').value='68';id('saveTarget').click();await settle();assert.equal(posts.at(-1).data.target_f,68);
id('stopButton').click();await settle();assert.equal(posts.at(-1).path,'/api/stop');assert.equal(state.operation,'stopped');assert(id('goButton').disabled);
id('tab-temperatures').click();for(let i=1;i<=6;i++)id('temp'+i).value=45+i*5;
id('temperatureForm').dispatchEvent(new window.Event('submit',{cancelable:true}));await settle();assert.equal(posts.at(-1).data.action,'temperatures');
id('temp3').value='40';const before=posts.length;id('temperatureForm').dispatchEvent(new window.Event('submit',{cancelable:true}));await settle();assert.equal(posts.length,before);assert(id('notice').textContent.includes('increase'));
// Stop remains effective while an automatic-start request is still in flight.
id('testButton').click();await settle();id('startingPulse').value='1300';id('sendPulse').click();await settle();
let finishGo;delayGo=new Promise(resolve=>{finishGo=resolve;});id('goButton').click();await settle();
const stopsBefore=posts.filter(p=>p.path==='/api/stop').length;
id('stopButton').click();await settle();assert.equal(state.operation,'stopped');finishGo();await settle();
assert(posts.filter(p=>p.path==='/api/stop').length>=stopsBefore+2,'Stop is repeated after the in-flight action finishes');assert.equal(state.operation,'stopped');
offline=true;await intervals.at(-1)();assert(id('testButton').disabled&&id('sendPulse').disabled&&id('goButton').disabled);assert.equal(id('encoderAngle').textContent,'—');assert.deepEqual(errors,[]);
window.happyDOM.abort();console.log('PASS: actual page script: no motion on load, unique IDs, manual/Go workflow, jog, known start, target, Stop, invalid temperatures and offline controls');
