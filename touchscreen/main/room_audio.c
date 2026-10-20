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
static atomic_bool preferences_dirty;
bool room_audio_notify_enabled(int key){return key>=3&&key<=6&&(atomic_load(&notify_mask)&(1U<<(key-3)))!=0;}

static bool tone(esp_codec_dev_handle_t codec,float hz,unsigned ms)
{
    int16_t pcm[256*2];unsigned total=16000*ms/1000,at=0;
    while(at<total){
        unsigned count=total-at<256?total-at:256;
        for(unsigned n=0;n<count;++n){
            unsigned i=at+n;
            float envelope=fminf(1.0f,fminf(i/220.0f,(total-i)/440.0f));
            int16_t sample=(int16_t)(32767.0f*envelope*sinf(6.2831853f*hz*i/16000.0f));
            pcm[n*2]=sample;pcm[n*2+1]=sample;
        }
        if(esp_codec_dev_write(codec,pcm,count*2*sizeof(*pcm))!=ESP_CODEC_DEV_OK)return false;
        at+=count;
    }
    return true;
}
static void save_preferences(void)
{
    if(!atomic_exchange(&preferences_dirty,false))return;
    nvs_handle_t nvs;esp_err_t err=nvs_open("room_alerts",NVS_READWRITE,&nvs);
    if(err==ESP_OK){err=nvs_set_u8(nvs,"notify",(uint8_t)atomic_load(&notify_mask));if(err==ESP_OK)err=nvs_commit(nvs);nvs_close(nvs);}
    if(err!=ESP_OK){atomic_store(&preferences_dirty,true);ESP_LOGW("room_audio","Could not persist notification preferences");}
}
static void audio_task(void *arg)
{
    esp_codec_dev_handle_t codec=NULL;cue_t cue;
    for(;;){
        if(xQueueReceive(cues,&cue,portMAX_DELAY)!=pdTRUE)continue;
        save_preferences();
        if(cue.key==-1)continue;
        /* Completion cues carry permission captured when the cycle ended.
         * The one-shot toggle is already off by the time playback starts. */
        if(esp_timer_get_time()-cue.at>5000000)continue;
        if(!codec){
            codec=bsp_audio_codec_speaker_init();
            esp_codec_dev_sample_info_t format={.sample_rate=16000,.channel=2,.bits_per_sample=16};
            if(!codec||esp_codec_dev_open(codec,&format)!=ESP_CODEC_DEV_OK){
                ESP_LOGW("room_audio","Speaker initialization failed; completion sound skipped");
                if(codec){esp_codec_dev_delete(codec);}codec=NULL;
                bsp_audio_poweramp_enable(false);continue;
            }
            int volume=esp_codec_dev_set_out_vol(codec,100);
            int mute=esp_codec_dev_set_out_mute(codec,true);
            esp_err_t pa=bsp_audio_poweramp_enable(false);
            ESP_LOGI("room_audio","Speaker configured: 16000 Hz stereo, volume=100, PCM peak=32767; volume_rc=%d mute_rc=%d amp_rc=0x%x",volume,mute,(unsigned)pa);
            if(volume!=ESP_CODEC_DEV_OK||mute!=ESP_CODEC_DEV_OK||pa!=ESP_OK){
                esp_codec_dev_close(codec);esp_codec_dev_delete(codec);codec=NULL;continue;
            }
        }
        esp_err_t amp=bsp_audio_poweramp_enable(true);
        int unmute=esp_codec_dev_set_out_mute(codec,false);
        if(amp!=ESP_OK||unmute!=ESP_CODEC_DEV_OK){
            ESP_LOGW("room_audio","Speaker enable failed: amp_rc=0x%x unmute_rc=%d",(unsigned)amp,unmute);
            esp_codec_dev_set_out_mute(codec,true);bsp_audio_poweramp_enable(false);continue;
        }
        /* Match the board example's amplifier startup time before tone data. */
        vTaskDelay(pdMS_TO_TICKS(200));
        float first=cue.key<5?659.25f:523.25f;
        bool ok=tone(codec,first,140)&&tone(codec,0,35)&&tone(codec,783.99f,140)&&tone(codec,0,35)&&tone(codec,1046.5f,200);
        /* A write queues DMA data; allow the silence tail to drain before mute. */
        ok=tone(codec,0,120)&&ok;
        vTaskDelay(pdMS_TO_TICKS(100));
        int muted=esp_codec_dev_set_out_mute(codec,true);
        esp_err_t disabled=bsp_audio_poweramp_enable(false);
        if(cue.key==-2)ESP_LOGI("room_audio","Speaker test: PCM %s; mute_rc=%d amp_off_rc=0x%x",ok?"sent":"failed",muted,(unsigned)disabled);
        else ESP_LOGI("room_audio","%s %d completion: PCM %s; mute_rc=%d amp_off_rc=0x%x",cue.key<5?"Washer":"Dryer",cue.key<5?cue.key-2:cue.key-4,ok?"sent":"failed",muted,(unsigned)disabled);
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
    if(!cues||key<3||key>6)return;
    unsigned bit=1U<<(key-3);
    unsigned previous=atomic_fetch_and(&notify_mask,~bit);
    if(!(previous&bit))return;
    /* Consume the opt-in immediately, even if playback subsequently fails. */
    atomic_store(&preferences_dirty,true);
    cue_t cue={.key=key,.at=esp_timer_get_time()};
    if(xQueueSend(cues,&cue,0)!=pdTRUE)ESP_LOGW("room_audio","Completion sound queue full; one-shot notification reset");
}
bool room_audio_test(void)
{
    if(!cues)return false;
    cue_t cue={.key=-2,.at=esp_timer_get_time()};
    return xQueueSend(cues,&cue,0)==pdTRUE;
}
bool room_audio_set_notify(int key,bool enabled)
{
    if(!cues||key<3||key>6)return false;
    unsigned bit=1U<<(key-3);
    if(enabled)atomic_fetch_or(&notify_mask,bit);
    else atomic_fetch_and(&notify_mask,~bit);
    atomic_store(&preferences_dirty,true);
    cue_t save={.key=-1};
    /* A full queue already wakes the worker, which saves the dirty mask. */
    xQueueSend(cues,&save,0);return true;
}
#else
bool room_audio_test(void){return false;}
bool room_audio_notify_enabled(int key){return false;}
bool room_audio_set_notify(int key,bool enabled){return false;}
void room_audio_start(void){}
void room_audio_laundry_done(int key){}
#endif
