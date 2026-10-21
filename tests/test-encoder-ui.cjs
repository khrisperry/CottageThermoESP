const fs=require('node:fs'), vm=require('node:vm'), assert=require('node:assert/strict');
const nodes={};
let encoder;
const context=vm.createContext({document:{getElementById:id=>nodes[id]??=( {textContent:'',style:{}} )},
 fetch:async path=>({ok:true,json:async()=>path==='/api/events'?[]:{simulation:true,encoder,target_f:65,reason:'hold',source:'none'}}),
 setInterval:()=>{}, console});
const code=fs.readFileSync('controller/main/ui.html','utf8').split('<script>')[1].split('</script>')[0].replace('setInterval(refresh,3000);load();','');
vm.runInContext(code,context);
(async()=>{
 for(const [values,text,angle] of [
  [{online:true,angle_valid:true,magnet_detected:true,angle_deg:180,raw_angle:2048},'Field OK','180.00°'],
  [{online:true,angle_valid:false,magnet_detected:false},'Magnet not detected','—'],
  [{online:true,angle_valid:false,magnet_detected:true,too_weak:true},'too weak','—'],
  [{online:true,angle_valid:false,magnet_detected:true,too_strong:true},'too strong','—'],
  [{online:false,angle_valid:false,error:'ESP_ERR_TIMEOUT'},'ESP_ERR_TIMEOUT','—'],
  [{online:true,angle_valid:true,magnet_detected:true,angle_deg:90,raw_angle:1024},'Field OK','90.00°']
 ]) {
  encoder={sda_gpio:6,scl_gpio:7,...values};
  await vm.runInContext('refresh()',context);
  assert.ok(nodes.encoderStatus.textContent.includes(text));
  assert.equal(nodes.encoderAngle.textContent,angle);
 }
 console.log('PASS: encoder webpage healthy, missing/weak/strong magnet, read failure and recovery');
})().catch(e=>{console.error(e);process.exitCode=1;});
