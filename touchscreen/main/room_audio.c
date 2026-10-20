/* Short locally synthesized completion chime. No stream, file or HA service. */
#include "room_audio.h"
#include "sdkconfig.h"
#if CONFIG_ROOM_NATIVE_DISPLAY
#include <math.h>
#include <stdint.h>
#include <stdatomic.h>
#include "nvs.h"
#include "bsp/esp-bsp.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
typedef struct {int key;int64_t at;} cue_t;
static QueueHandle_t cues;
static atomic_uint notify_mask;
bool room_audio_notify_enabled(int key){return key>=3&&key<=6&&(atomic_load(&notify_mask)&(1U<<(key-3)))!=0;}

static bool tone(esp_codec_dev_handle_t codec,float hz,unsigned ms)
{
    int16_t pcm[256];unsigned total=22050*ms/1000,at=0;
    while(at<total){
        unsigned count=total-at<256?total-at:256;
        for(unsigned n=0;n<count;++n){
            unsigned i=at+n;
            float envelope=fminf(1.0f,fminf(i/220.0f,(total-i)/440.0f));
            pcm[n]=(int16_t)(2500*envelope*sinf(6.2831853f*hz*i/22050.0f));
        }
        if(esp_codec_dev_write(codec,pcm,count*sizeof(*pcm))!=ESP_CODEC_DEV_OK)return false;
        at+=count;
    }
    return true;
}
static void audio_task(void *arg)
{
    esp_codec_dev_handle_t codec=NULL;cue_t cue;
    for(;;){
        if(xQueueReceive(cues,&cue,portMAX_DELAY)!=pdTRUE)continue;
        if(cue.key==-1){
            nvs_handle_t nvs;esp_err_t err=nvs_open("room_alerts",NVS_READWRITE,&nvs);
            if(err==ESP_OK){err=nvs_set_u8(nvs,"notify",(uint8_t)atomic_load(&notify_mask));if(err==ESP_OK)err=nvs_commit(nvs);nvs_close(nvs);}
            if(err!=ESP_OK)ESP_LOGW("room_audio","Could not persist notification preferences");
            continue;
        }
        if(!room_audio_notify_enabled(cue.key)||esp_timer_get_time()-cue.at>5000000)continue;
        if(!codec){
            codec=bsp_audio_codec_speaker_init();
            esp_codec_dev_sample_info_t format={.sample_rate=22050,.channel=1,.bits_per_sample=16};
            if(!codec||esp_codec_dev_open(codec,&format)!=ESP_CODEC_DEV_OK){
                ESP_LOGW("room_audio","Speaker initialization failed; completion sound skipped");
                if(codec){esp_codec_dev_delete(codec);}codec=NULL;
                bsp_audio_poweramp_enable(false);continue;
            }
            esp_codec_dev_set_out_vol(codec,30);esp_codec_dev_set_out_mute(codec,true);bsp_audio_poweramp_enable(false);
        }
        if(!room_audio_notify_enabled(cue.key))continue;
        bsp_audio_poweramp_enable(true);esp_codec_dev_set_out_mute(codec,false);
        float first=cue.key<5?659.25f:523.25f;
        bool ok=tone(codec,first,140)&&tone(codec,0,35)&&tone(codec,783.99f,140)&&tone(codec,0,35)&&tone(codec,1046.5f,200);
        tone(codec,0,80);
        esp_codec_dev_set_out_mute(codec,true);bsp_audio_poweramp_enable(false);
        ESP_LOGI("room_audio","%s %d completion chime %s",cue.key<5?"Washer":"Dryer",cue.key<5?cue.key-2:cue.key-4,ok?"finished":"failed");
    }
}
void room_audio_start(void)
{
    /* Independent appliance preferences; notifications are opt-in. */
    nvs_handle_t nvs;uint8_t saved=0;
    if(nvs_open("room_alerts",NVS_READONLY,&nvs)==ESP_OK){nvs_get_u8(nvs,"notify",&saved);nvs_close(nvs);}
    atomic_store(&notify_mask,saved&15U);
    cues=xQueueCreate(8,sizeof(cue_t));
    if(!cues){ESP_LOGW("room_audio","Completion audio queue unavailable");return;}
    if(xTaskCreate(audio_task,"room_audio",6144,NULL,2,NULL)!=pdPASS){
        vQueueDelete(cues);cues=NULL;ESP_LOGW("room_audio","Completion audio task unavailable");
    }
}
void room_audio_laundry_done(int key)
{
    if(!cues||!room_audio_notify_enabled(key))return;
    cue_t cue={.key=key,.at=esp_timer_get_time()};
    if(xQueueSend(cues,&cue,0)!=pdTRUE)ESP_LOGW("room_audio","Completion sound queue full; cue skipped");
}
bool room_audio_set_notify(int key,bool enabled)
{
    if(!cues||key<3||key>6)return false;
    unsigned bit=1U<<(key-3),previous=atomic_load(&notify_mask);
    unsigned next=enabled?previous|bit:previous&~bit;
    atomic_store(&notify_mask,next);
    cue_t save={.key=-1};
    if(xQueueSend(cues,&save,0)!=pdTRUE){atomic_store(&notify_mask,previous);return false;}
    return true;
}
#else
bool room_audio_notify_enabled(int key){return false;}
bool room_audio_set_notify(int key,bool enabled){return false;}
void room_audio_start(void){}
void room_audio_laundry_done(int key){}
#endif
