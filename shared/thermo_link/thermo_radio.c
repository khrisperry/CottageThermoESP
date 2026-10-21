/* No allocation, blocking or application work in the Wi-Fi callback. */
#include "thermo_radio.h"
#include "sdkconfig.h"
#include <string.h>
#include <stdatomic.h>
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "psa/crypto.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
static QueueHandle_t requests,replies;
static SemaphoreHandle_t io;
static uint8_t peer_mac[6];
static mbedtls_svc_key_id_t auth_key;
typedef struct {tl_packet_t packet;uint8_t tag[16];} wire_t;
typedef struct {wire_t wire;uint8_t channel;} received_t;
static atomic_uint known_channel;
static bool sign(const tl_packet_t *p,uint8_t tag[16])
{
    uint8_t hash[32];size_t length=0;
    if(psa_mac_compute(auth_key,PSA_ALG_HMAC(PSA_ALG_SHA_256),(const uint8_t*)p,sizeof(*p),hash,sizeof(hash),&length)!=PSA_SUCCESS||length!=32)return false;
    memcpy(tag,hash,16);memset(hash,0,sizeof(hash));return true;
}
static bool authentic(const wire_t *w)
{
    uint8_t tag[16];if(!sign(&w->packet,tag))return false;
    unsigned diff=0;for(unsigned n=0;n<16;++n)diff|=tag[n]^w->tag[n];return diff==0;
}
static esp_err_t send_packet(const tl_packet_t *p)
{
    wire_t w={.packet=*p};if(!sign(p,w.tag))return ESP_FAIL;
    return esp_now_send(peer_mac,(const uint8_t*)&w,sizeof(w));
}
static tl_radio_handler_t serve;
static atomic_bool ready;
/* 32-bit monotonic ticks avoid non-lock-free 64-bit atomics on Xtensa. */
static atomic_uint last_reply_ms;
static uint32_t millis(void){return (uint32_t)(esp_timer_get_time()/1000);}
bool tl_radio_configured(void){return ready;}
bool tl_radio_online(void){unsigned at=last_reply_ms;return ready&&at&&(uint32_t)(millis()-at)<8000;}
static void receive(const esp_now_recv_info_t *info,const uint8_t *data,int size)
{
    if(!ready||!info||!info->src_addr||size!=(int)sizeof(wire_t)||memcmp(info->src_addr,peer_mac,6))return;
    received_t rx={0};memcpy(&rx.wire,data,sizeof(rx.wire));
    if(!tl_valid(&rx.wire.packet))return;
    if(info->rx_ctrl)rx.channel=info->rx_ctrl->channel;
    if(rx.wire.packet.operation==TL_REPLY)xQueueSend(replies,&rx,0);
    else if(serve)xQueueSend(requests,&rx,0);
}
static void server_task(void *arg)
{
    received_t rx;tl_packet_t r;
    for(;;)if(xQueueReceive(requests,&rx,portMAX_DELAY)==pdTRUE){
        if(!authentic(&rx.wire))continue;
        known_channel=rx.channel;serve(&rx.wire.packet,&r);
        if(send_packet(&r)==ESP_OK)last_reply_ms=millis();
    }
}
void tl_radio_start(tl_radio_handler_t handler)
{
#if CONFIG_COTTAGE_ESPNOW
    const char *mac=CONFIG_COTTAGE_ESPNOW_PEER,*secret=CONFIG_COTTAGE_ESPNOW_KEY;
    uint8_t key[16];bool valid=strlen(mac)==17&&strlen(secret)==32;
    for(unsigned i=0;valid&&i<6;++i){int a=tl_digit(mac[i*3]),b=tl_digit(mac[i*3+1]);valid=a>=0&&b>=0&&(i==5||mac[i*3+2]==':');peer_mac[i]=(a>=0&&b>=0)?(a<<4)|b:0;}
    for(unsigned i=0;valid&&i<16;++i){int a=tl_digit(secret[i*2]),b=tl_digit(secret[i*2+1]);valid=a>=0&&b>=0;key[i]=(a>=0&&b>=0)?(a<<4)|b:0;}
    if(!valid||(peer_mac[0]&1)){ESP_LOGE("thermo_radio","Fallback disabled: configure unicast station MAC and 32 hex key");return;}
    requests=xQueueCreate(8,sizeof(received_t));replies=xQueueCreate(8,sizeof(received_t));io=xSemaphoreCreateMutex();
    ESP_ERROR_CHECK(requests&&replies&&io?ESP_OK:ESP_ERR_NO_MEM);
    serve=handler;ESP_ERROR_CHECK(esp_now_init());ESP_ERROR_CHECK(esp_now_set_pmk(key));
    esp_now_peer_info_t peer={.ifidx=WIFI_IF_STA,.channel=0,.encrypt=true};
    memcpy(peer.peer_addr,peer_mac,6);memcpy(peer.lmk,key,16);
    ESP_ERROR_CHECK(psa_crypto_init()==PSA_SUCCESS?ESP_OK:ESP_FAIL);
    psa_key_attributes_t attributes=PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attributes,PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attributes,PSA_ALG_HMAC(PSA_ALG_SHA_256));
    psa_set_key_type(&attributes,PSA_KEY_TYPE_HMAC);psa_set_key_bits(&attributes,128);
    psa_status_t imported=psa_import_key(&attributes,key,sizeof(key),&auth_key);
    psa_reset_key_attributes(&attributes);memset(key,0,sizeof(key));
    ESP_ERROR_CHECK(imported==PSA_SUCCESS?ESP_OK:ESP_FAIL);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));ESP_ERROR_CHECK(esp_now_register_recv_cb(receive));ready=true;
    if(serve)ESP_ERROR_CHECK(xTaskCreate(server_task,"direct_link",4096,NULL,3,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM);
    ESP_LOGI("thermo_radio","Encrypted paired ESP-NOW ready");
#else
    (void)handler;
#endif
}
esp_err_t tl_radio_exchange(const tl_packet_t *p,tl_packet_t *out)
{
    if(!ready||!io)return ESP_ERR_INVALID_STATE;
    if(xSemaphoreTake(io,pdMS_TO_TICKS(1500))!=pdTRUE)return ESP_ERR_TIMEOUT;
    received_t rx;while(xQueueReceive(replies,&rx,0)==pdTRUE){}
    esp_err_t err=send_packet(p);
    TickType_t started=xTaskGetTickCount();
    while(err==ESP_OK){
        TickType_t elapsed=xTaskGetTickCount()-started;
        if(elapsed>=pdMS_TO_TICKS(1200)){err=ESP_ERR_TIMEOUT;break;}
        if(xQueueReceive(replies,&rx,pdMS_TO_TICKS(1200)-elapsed)!=pdTRUE){err=ESP_ERR_TIMEOUT;break;}
        tl_packet_t r=rx.wire.packet;
        if(authentic(&rx.wire)&&r.client==p->client&&r.sequence==p->sequence){
            *out=r;known_channel=rx.channel;last_reply_ms=millis();break;
        }
    }
    xSemaphoreGive(io);return err;
}
void tl_radio_search_tick(void)
{
    static uint32_t last_hop;static uint8_t channel=1;
    if(!ready)return;
    wifi_sta_list_t clients={0};
    if(esp_wifi_ap_get_sta_list(&clients)==ESP_OK&&clients.num)return;
    if(tl_radio_online()){
        uint8_t current=0;wifi_second_chan_t secondary;
        if(known_channel&&esp_wifi_get_channel(&current,&secondary)==ESP_OK&&current!=known_channel)
            esp_wifi_set_channel(known_channel,WIFI_SECOND_CHAN_NONE);
        return;
    }
    if((uint32_t)(millis()-last_hop)<(serve?2300U:1700U))return;
    /* An attached setup client owns the channel until it disconnects. */
    if(esp_wifi_set_channel(channel,WIFI_SECOND_CHAN_NONE)==ESP_OK){
        last_hop=millis();channel=channel==11?1:channel+1;
    }
}
