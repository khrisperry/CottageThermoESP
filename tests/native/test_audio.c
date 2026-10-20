/* Production room_audio.c, fake codec/RTOS/NVS; no hardware or network. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "audio_host.h"
#include "../../touchscreen/main/room_audio.c"
static cue_t queue[8];static unsigned queued,head;
static int64_t clock_us=1000000,amp_started;
static jmp_buf stop_worker;
static uint8_t persisted;
static bool amp_on,muted=true,write_failure,amp_failure;
static unsigned writes,nonzero_writes,open_count,startup_delays,drain_delays,peak;
void audio_test_log(const char *tag,const char *fmt,...){}
int64_t esp_timer_get_time(void){return clock_us;}
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *h){assert(!strcmp(name,"room_alerts"));*h=1;return ESP_OK;}
esp_err_t nvs_get_u8(nvs_handle_t h,const char *key,uint8_t *v){*v=persisted;return ESP_OK;}
esp_err_t nvs_set_u8(nvs_handle_t h,const char *key,uint8_t v){persisted=v;return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h){return ESP_OK;}
void nvs_close(nvs_handle_t h){}
QueueHandle_t xQueueCreate(unsigned count,unsigned size){assert(count==8&&size==sizeof(cue_t));return (void*)1;}
int xQueueSend(QueueHandle_t h,const void *p,int timeout){if(queued==8)return 0;queue[(head+queued)%8]=*(const cue_t*)p;++queued;return pdTRUE;}
int xQueueReceive(QueueHandle_t h,void *p,int timeout){if(!queued)longjmp(stop_worker,1);*(cue_t*)p=queue[head];head=(head+1)%8;--queued;return pdTRUE;}
void vQueueDelete(QueueHandle_t h){}
void vTaskDelay(int ms){if(ms==200)++startup_delays;if(ms==100)++drain_delays;clock_us+=(int64_t)ms*1000;}
int xTaskCreate(void(*fn)(void*),const char *name,unsigned stack,void *p,unsigned priority,void *task){return pdPASS;}
esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void){return (void*)2;}
esp_err_t bsp_audio_poweramp_enable(bool on){if(on&&amp_failure)return -1;amp_on=on;if(on)amp_started=clock_us;return ESP_OK;}
int esp_codec_dev_open(esp_codec_dev_handle_t h,esp_codec_dev_sample_info_t *f){assert(f->sample_rate==16000&&f->channel==2&&f->bits_per_sample==16);++open_count;return ESP_CODEC_DEV_OK;}
int esp_codec_dev_close(esp_codec_dev_handle_t h){return ESP_CODEC_DEV_OK;}
void esp_codec_dev_delete(esp_codec_dev_handle_t h){}
int esp_codec_dev_set_out_vol(esp_codec_dev_handle_t h,int volume){assert(volume==75);return ESP_CODEC_DEV_OK;}
int esp_codec_dev_set_out_mute(esp_codec_dev_handle_t h,bool mute){muted=mute;return ESP_CODEC_DEV_OK;}
int esp_codec_dev_write(esp_codec_dev_handle_t h,void *data,int len)
{
 assert(amp_on&&!muted&&len>0&&len%4==0);
 if(write_failure){write_failure=false;return -1;}
 ++writes;int16_t *p=data;bool audible=false;
 for(int n=0;n<len/2;n+=2){assert(p[n]==p[n+1]);unsigned value=p[n]<0?-(int)p[n]:p[n];if(value>peak)peak=value;if(value)audible=true;}
 if(audible){++nonzero_writes;assert(clock_us-amp_started>=200000);}
 clock_us+=(int64_t)(len/4)*1000000/16000;return ESP_CODEC_DEV_OK;
}
static void run_worker(void){if(!setjmp(stop_worker))audio_task(NULL);}
int main(void)
{
 room_audio_start();assert(!room_audio_notify_enabled(3)&&!room_audio_notify_enabled(5));
 room_audio_laundry_done(3);assert(!queued);
 assert(room_audio_set_notify(3,true));assert(room_audio_set_notify(5,true));
 room_audio_laundry_done(3);assert(!room_audio_notify_enabled(3)&&room_audio_notify_enabled(5));
 run_worker();assert(persisted==4&&writes>0&&nonzero_writes>0&&peak>=5000&&peak<=6000);
 assert(muted&&!amp_on&&startup_delays==1&&drain_delays==1);
 unsigned before=writes;room_audio_laundry_done(3);assert(!queued&&writes==before);
 room_audio_laundry_done(5);assert(!room_audio_notify_enabled(5));run_worker();assert(persisted==0&&writes>before);
 before=writes;assert(room_audio_test());run_worker();assert(writes>before&&persisted==0); /* explicit test ignores opt-in */
 write_failure=true;assert(room_audio_test());run_worker();assert(muted&&!amp_on);
 before=writes;amp_failure=true;assert(room_audio_test());run_worker();assert(writes==before&&muted&&!amp_on);amp_failure=false;
 assert(room_audio_set_notify(3,true));run_worker();
 for(unsigned n=0;n<8;++n)assert(room_audio_set_notify(5,false)); /* fill queue */
 room_audio_laundry_done(3);assert(!room_audio_notify_enabled(3));run_worker();assert(persisted==0); /* reset persists even if sound queue full */
 assert(open_count>=4);
 puts("PASS: production audio stereo PCM/level/startup/drain, failure mute, explicit speaker test, per-cycle auto-off and NVS preference writes");
 return 0;
}
