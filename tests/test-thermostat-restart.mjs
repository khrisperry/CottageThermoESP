import fs from 'node:fs';
import assert from 'node:assert/strict';
import {Window} from 'happy-dom';
const window=new Window({url:'http://thermostat.test',settings:{enableJavaScriptEvaluation:true}});
const posts=[],intervals=[],errors=[];
let boot=1,fail=false,completeRequest=null,upload;
window.setInterval=fn=>{intervals.push(fn);return intervals.length;};
window.confirm=()=>true;
window.addEventListener('error',e=>errors.push(e.message));
window.fetch=async(path,opts={})=>{
 if(opts.method==='POST'){
  posts.push({path,data:JSON.parse(opts.body)});
  if(completeRequest)await completeRequest;
  return {ok:!fail,json:async()=>({message:fail?'A firmware update is in progress.':'Thermostat restarting.'})};
 }
 return {ok:true,json:async()=>path==='/api/settings'?{}:{services:[],summary:'Online',summary_kind:'ok',firmware:{boot_id:boot,version:'0.15.1',slot_bytes:4194304}}};
};
window.XMLHttpRequest=class {
 constructor(){this.upload={};upload=this;}
 open(){} setRequestHeader(){} send(){}
};
const html=fs.readFileSync(new URL('../touchscreen/main/settings.html',import.meta.url),'utf8');
window.document.write(html.replace(/<script>[\s\S]*?<\/script>/,''));
window.eval(html.match(/<script>([\s\S]*?)<\/script>/)[1]);
const id=x=>window.document.getElementById(x);
const settle=()=>new Promise(r=>setTimeout(r,20));
await settle();
[...id('nav').querySelectorAll('button')].find(b=>b.textContent==='System').click();
assert(id('restartThermostat')&&!id('restartThermostat').disabled);
assert.equal(posts.length,0,'Rendering must not trigger a restart');
window.confirm=()=>false;id('restartThermostat').click();await settle();assert.equal(posts.length,0);
window.confirm=()=>true;
let finish;completeRequest=new Promise(resolve=>{finish=resolve;});
id('restartThermostat').click();await settle();
assert(id('restartThermostat').disabled&&id('uploadFirmware').disabled);
await intervals[0]();assert(id('restartThermostat').disabled,'Poll cannot re-enable an in-flight restart');
id('restartThermostat').click();assert.equal(posts.length,1);
finish();await settle();completeRequest=null;
assert.deepEqual(posts[0],{path:'/api/settings',data:{section:'restart',confirm:'restart_thermostat'}});
await intervals[0]();assert(id('restartThermostat').disabled,'Old boot is not a completed restart');
++boot;await intervals[0]();
assert(!id('restartThermostat').disabled&&id('message').textContent.includes('Device is back online'));
fail=true;id('restartThermostat').click();await settle();
assert(!id('restartThermostat').disabled&&id('message').textContent.includes('could not be confirmed'));
fail=false;
Object.defineProperty(id('firmware'),'files',{value:[{name:'thermostat.bin',size:2048}]});
id('firmware').form.dispatchEvent(new window.Event('submit',{cancelable:true}));
assert(upload&&id('restartThermostat').disabled);
const count=posts.length;id('restartThermostat').click();assert.equal(posts.length,count,'No restart during an upload');
assert.deepEqual(errors,[]);window.happyDOM.abort();
console.log('PASS: thermostat web restart, cancellation, one request, pending/upload guards, boot-aware reconnection and error recovery');
