const assert=require('node:assert/strict');const http=require('node:http');const fs=require('node:fs');const path=require('node:path');const {chromium}=require('playwright');
(async()=>{
 const html=fs.readFileSync(path.join(__dirname,'../touchscreen/main/settings.html'),'utf8');const requests=[];let boot=1,version='0.11.0',heat=false;
 const cfg={ha_url:'https://ha.example',token_configured:true,controller:'http://192.0.2.20',controller_token_configured:true,ships_enabled:true,ship_url:'https://ships.example',ship_range:30,mqtt:{enabled:false,managed:false,broker:'',username:'',password_configured:false,prefix:'homeassistant',name:'Cottage thermostat'},radio:{enabled:false,peer:'',key_configured:false},volume:100,camera_fill:false};
 const status=()=>({summary:heat?'Online':'Heat Offline',summary_kind:heat?'ok':'offline',services:[{name:'Wi-Fi',state:'Connected',kind:'ok'},{name:'Home Assistant',state:'Online',kind:'ok'},{name:'Heat controller',state:heat?'Online':'Offline',kind:heat?'ok':'offline'},{name:'ESP-NOW',state:'Not configured',kind:'unconfigured'},{name:'Local sensor',state:'Fresh',kind:'ok'},{name:'Ship tracking',state:'Online',kind:'ok'},{name:'MQTT climate',state:cfg.mqtt.enabled?'Connected':'Not configured',kind:cfg.mqtt.enabled?'ok':'unconfigured'}],temperature_f:67.1,target_f:68,performance:{frame_ms:4.2,max_frame_ms:12,lvgl_stack_min_free:12000,free_internal:100000,largest_internal:80000,free_psram:4000000},ships:{service_online:true,feed_online:true,last_error:'',count:3,nearby:14,request_ms:72,bytes:6765,failures:0},integration_sync:'Applied',firmware:{version,boot_id:boot,board:'Waveshare 4B',running_slot:'factory',slot_bytes:4194304}});
 const server=http.createServer(async(req,res)=>{const chunks=[];for await(const c of req)chunks.push(c);const body=Buffer.concat(chunks);res.setHeader('Cache-Control','no-store');
  if(req.url==='/'){res.setHeader('Content-Type','text/html');res.end(html);return;}
  res.setHeader('Content-Type','application/json');const reply=(code,j)=>{res.statusCode=code;res.end(JSON.stringify(j));};
  if(req.url==='/api/device/status'){reply(200,status());return;}

  if(req.url==='/api/settings'&&req.method==='GET'){reply(200,cfg);return;}
  assert.equal(req.headers.origin,`http://${req.headers.host}`);
  if(req.url==='/api/firmware'){assert.equal(req.headers['content-type'],'application/octet-stream');assert.equal(body.length,2048);requests.push({section:'firmware'});setTimeout(()=>{boot++;version='0.11.1'},200);reply(200,{message:'Firmware validated. Restarting...'});return;}
  const p=JSON.parse(body);requests.push(p);
  if(p.section==='mqtt'){cfg.mqtt={...cfg.mqtt,...p};heat=true;setTimeout(()=>boot++,200);}
  if(p.section==='ships'){cfg.ship_range=p.range;cfg.ship_url=p.url;setTimeout(()=>boot++,200);}
  reply(200,{message:['sound','sound_test','mqtt_test'].includes(p.section)?'Request accepted':'Saved. Restarting display...'});
 });await new Promise(r=>server.listen(0,'127.0.0.1',r));
 const executablePath=process.env.CHROMIUM_PATH;const browser=await chromium.launch({headless:true,...(executablePath?{executablePath}:{}),args:['--no-sandbox']});const page=await browser.newPage({viewport:{width:1440,height:1080}});const errors=[];page.on('pageerror',e=>errors.push(e.message));
 try{
  await page.goto(`http://127.0.0.1:${server.address().port}`);await page.getByText('Heat Offline',{exact:true}).waitFor();
  assert.equal(await page.locator('#admin').count(),0);assert.equal(await page.locator('#unlockCard').count(),0);
  await page.getByRole('button',{name:'MQTT climate',exact:true}).click();await page.locator('#brokerHost').waitFor();assert(await page.getByRole('button',{name:'Test saved connection'}).isDisabled());
  await page.locator('#enabled').check();await page.locator('#brokerHost').fill('192.168.60.63');await page.locator('#username').fill('mqtt-user');await page.locator('#password').fill('typed-secret');await page.locator('#content .secret button').click();assert.equal(await page.locator('#password').getAttribute('type'),'text');await page.locator('#content .secret button').click();
  await page.locator('#content form').getByRole('button',{name:'Save and restart display'}).click();await page.getByText(/Device is back online/).waitFor({timeout:10000});assert.equal(requests.at(-1).broker,'mqtt://192.168.60.63:1883');assert.equal(requests.at(-1).password,'typed-secret');assert.equal(await page.locator('#password').inputValue(),'');
  await page.screenshot({path:path.join(__dirname,'test-results/device-mqtt-desktop.png'),fullPage:true});
  await page.getByRole('button',{name:'Ship tracking',exact:true}).click();await page.locator('#range').selectOption('100');await page.locator('#content form').getByRole('button',{name:'Save and restart display'}).click();await page.getByText(/Device is back online/).waitFor({timeout:10000});assert.equal(requests.at(-1).range,100);
  await page.getByRole('button',{name:'Sound & display',exact:true}).click();await page.locator('#volume').evaluate(e=>{e.value='80';e.dispatchEvent(new Event('input'))});await page.getByRole('button',{name:'Test sound',exact:true}).click();await page.waitForTimeout(100);assert.equal(requests.at(-2).volume,80);assert.equal(requests.at(-1).section,'sound_test');
  await page.getByRole('button',{name:'System',exact:true}).click();await page.locator('#firmware').setInputFiles({name:'cottage_thermo_room_4b.bin',mimeType:'application/octet-stream',buffer:Buffer.alloc(2048,0xe9)});await page.getByRole('button',{name:'Upload firmware',exact:true}).click();await page.getByText(/Running firmware 0.11.1/).waitFor({timeout:10000});assert(requests.some(x=>x.section==='firmware'));
  await page.setViewportSize({width:390,height:844});await page.getByRole('button',{name:'Overview',exact:true}).click();await page.screenshot({path:path.join(__dirname,'test-results/device-mobile.png'),fullPage:true});
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true);assert.deepEqual(errors,[]);
  console.log('PASS: device dashboard status/no-PIN, responsive layout, MQTT/ships/sound saves, secret clearing, upload progress/reboot flow; no page errors');
 }finally{await browser.close();await new Promise(r=>server.close(r));}
})().catch(e=>{console.error(e);process.exitCode=1});
