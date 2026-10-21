// Optional browser smoke test: npm install in tests/, then npm test.
const {chromium}=require('playwright');
const fs=require('node:fs');const path=require('node:path');const assert=require('node:assert/strict');
(async()=>{
 const browser=await chromium.launch({headless:true,executablePath:process.env.TEST_BROWSER_PATH||undefined,args:['--no-sandbox']});
 try {
 const page=await browser.newPage({viewport:{width:390,height:844}});let errors=[],posts=[];
 page.on('pageerror',e=>errors.push(e.message));
 const config={mode:'hold',target_f:65,direct_level:3,secondary_enabled:false,map:[],schedule:[],schedule_enabled:false};
 const status={simulation:true,mode:'hold',target_f:65,reason:'hold',source:'none',position_measured:false,pwm_active:false,bench_armed:false};
 await page.route('http://thermo.test/**',async route=>{
  const req=route.request();let p=new URL(req.url()).pathname;
  if(p==='/')return route.fulfill({contentType:'text/html',body:fs.readFileSync(path.join(__dirname,'../controller/main/ui.html'),'utf8')});
  if(req.headers()['x-thermo-token']!=='test-token')return route.fulfill({status:401,body:'API token required'});
  if(req.method()==='POST'){posts.push({p,body:req.postDataJSON()});return route.fulfill({contentType:'application/json',body:'{"ok":true}'});}
  return route.fulfill({contentType:'application/json',body:JSON.stringify(p==='/api/config'?config:p==='/api/events'?[{uptime_ms:0,message:'Boot: PWM disabled'}]:status)});
 });
 await page.goto('http://thermo.test');await page.locator('#token').fill('test-token');await page.locator('#connect').click();
 await page.getByText('SIMULATION · automatic PWM disabled',{exact:true}).waitFor();
 assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true,'mobile page should not overflow');
 await page.locator('#mode').selectOption('thermostat');await page.locator('#setpoint').fill('68');
 await page.getByRole('button',{name:'Save settings',exact:true}).click();
 await page.waitForFunction(()=>document.getElementById('message').textContent==='Saved / accepted');
 assert.equal(posts.at(-1).body.mode,'thermostat');assert.equal(posts.at(-1).body.target_f,68);
 await page.getByRole('button',{name:'Stop PWM & Hold',exact:true}).click();
 await page.waitForTimeout(50);assert.equal(posts.at(-1).p,'/api/stop');
 await page.locator('#schedule').fill('[{"days":127,"minute":360,"target_f":68}]');
 await page.locator('#scheduleEnabled').check();await page.getByRole('button',{name:'Save schedule',exact:true}).click();
 await page.waitForTimeout(50);assert.equal(posts.at(-1).body.schedule[0].minute,360);
 await page.locator('#token').fill('wrong');await page.getByRole('button',{name:'Connect',exact:true}).click();
 await page.getByText('API token required',{exact:true}).waitFor();
 assert.deepEqual(errors,[]);await page.close();
 const room=await browser.newPage();
 room.on('pageerror',e=>errors.push(e.message));let offline=false;
 await room.route('http://room.test/**',async route=>{
  const p=new URL(route.request().url()).pathname;
  if(p==='/')return route.fulfill({contentType:'text/html',body:fs.readFileSync(path.join(__dirname,'../touchscreen/main/room.html'),'utf8')});
  if(offline)return route.fulfill({status:503,body:'Controller unavailable'});
  return route.fulfill({contentType:'application/json',body:JSON.stringify({...status,source:'room',temperature_f:62.3})});
 });
 await room.goto('http://room.test');await room.locator('#token').fill('test-token');await room.getByRole('button',{name:'Connect',exact:true}).click();
 await room.getByText('62.3°F',{exact:true}).waitFor();offline=true;await room.evaluate(()=>refresh());
 assert.equal(await room.locator('#temp').textContent(),'—');assert.deepEqual(errors,[]);
 console.log('PASS: mobile controller UI, pairing error, settings/Stop/schedule actions, room view and offline state');
 } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exit(1)});
