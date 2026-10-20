const {chromium}=require('playwright');
const fs=require('node:fs');const path=require('node:path');const assert=require('node:assert/strict');
(async()=>{
 const browser=await chromium.launch({headless:true,executablePath:process.env.TEST_BROWSER_PATH||undefined,args:['--no-sandbox']});
 try{
 const page=await browser.newPage({viewport:{width:390,height:844}});let errors=[],commands=[],frames=0,offline=false,ambiguous=false;
 page.on('pageerror',e=>errors.push(e.message));
 let items=[['deck','light','off',['on','off']],['breezeway','light','on',['on','off']],['blinds','cover','open',['open','close','stop']],['kids_heat','switch','off',['on','off']],['kids_temp','temperature','65.2',[]],['vacuum','vacuum','docked',['start','stop']],['washer1','laundry','Running',[]],['washer2','laundry','Idle',[]],['dryer1','laundry','Finished',[]],['dryer2','laundry','Idle',[]],['camera','camera','idle',[]]].map(([key,kind,state,actions])=>({key,kind,state,actions,available:true,unit:kind==='temperature'?'°F':'',value:kind==='temperature'?65.2:undefined}));
 await page.route('http://home.test/**',async route=>{
   const req=route.request(),p=new URL(req.url()).pathname;
   if(p==='/')return route.fulfill({contentType:'text/html',body:fs.readFileSync(path.join(__dirname,'../ha_bridge/home.html'),'utf8')});
   if(req.headers()['x-cottage-token']!=='test-token')return route.fulfill({status:401,contentType:'application/json',body:'{"error":"Bridge token required"}'});
   if(offline)return route.fulfill({status:503,contentType:'application/json',body:'{"error":"Offline"}'});
   if(p==='/api/ha/status')return route.fulfill({contentType:'application/json',body:JSON.stringify({configured:true,connected:true,error:null,room_client:true,items})});
   if(p==='/api/ha/command'){commands.push(req.postDataJSON());return route.fulfill({contentType:'application/json',body:JSON.stringify(ambiguous?{accepted:false,outcome:'unknown',message:'Command outcome unknown; check device state'}:{accepted:true})});}
   if(p==='/api/ha/camera'){frames++;return route.fulfill({contentType:'image/jpeg',body:Buffer.from([255,216,255,217])});}
   return route.fulfill({status:404,body:'Not found'});
 });
 await page.goto('http://home.test');await page.locator('#token').fill('test-token');await page.locator('#connect').click();
 await page.getByText('Connected',{exact:true}).waitFor();assert.equal(await page.locator('#kids_temp-state').textContent(),'65.2 °F');
 assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true,'mobile overflow');
 await page.getByRole('button',{name:'Heat on',exact:true}).click();await page.waitForTimeout(100);
 assert.equal(commands.at(-1).key,'kids_heat');assert.equal(commands.at(-1).action,'on');assert.ok(commands.at(-1).request_id);
 await page.locator('[data-key="blinds"][data-action="stop"]').click();await page.waitForTimeout(100);assert.equal(commands.at(-1).action,'stop');
 ambiguous=true;await page.getByRole('button',{name:'Heat off',exact:true}).click();await page.waitForTimeout(100);assert.match(await page.locator('#message').textContent(),/outcome unknown/);
 await page.locator('[data-page="laundry"]').click();assert.equal(await page.locator('#washer1-state').textContent(),'Running');assert.equal(await page.locator('#dryer1-state').textContent(),'Finished');
 assert.equal(frames,0,'camera must not load in Home/Laundry');
 await page.locator('[data-page="camera"]').click();await page.waitForTimeout(200);assert.ok(frames>0);
 await page.locator('#cameraClose').click();const stopped=frames;await page.waitForTimeout(1200);assert.equal(frames,stopped,'camera requests stop when leaving');
 assert.equal(await page.locator('#frame').getAttribute('src'),null);
 offline=true;await page.evaluate(()=>refresh());
 assert.equal(await page.locator('#connection').textContent(),'Disconnected');assert.equal(await page.locator('#kids_temp-state').textContent(),'Unavailable');
 assert.equal(await page.locator('[data-key]:enabled').count(),0);
 assert.deepEqual(errors,[]);console.log('PASS: mobile HA panel, exact command selection, laundry states, on-demand camera, offline controls and cleared stale values');
 }finally{await browser.close()}
})().catch(e=>{console.error(e);process.exit(1)});
