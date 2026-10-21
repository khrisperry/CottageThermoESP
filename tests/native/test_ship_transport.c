/* Actual ship client transport/state. No requests leave this fixture. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../touchscreen/main/room_ships.c"
struct fake_client {void *user;esp_err_t (*handler)(esp_http_client_event_t*);int status;char url[384];char id[64],firmware[40],uptime[32],heap[32],rssi[32];};
static struct fake_client client;
static unsigned inits,attempts,cleanups,workers;static int64_t clock_us=1000000,handshake_us=12000000,chunk_delay;
static bool wifi=true,radio=false,updating=false,fail=false,complete=true;static size_t payload_size=2;static int next_code=200;
int64_t esp_timer_get_time(void){return clock_us;}
time_t time(time_t *out){return 1800000000;}
const char *room_settings_ship_url(void){return "https://ships.example";}
unsigned room_settings_ship_range(void){return 30;}
bool room_settings_ships_enabled(void){return true;}
bool room_wifi_connected(void){return wifi;}
bool room_controller_fallback(void){return radio;}
bool room_ota_busy(void){return updating;}
size_t heap_caps_get_free_size(int caps){return 120000;}
void *heap_caps_malloc(size_t n,int caps){return malloc(n);}
void *xSemaphoreCreateMutex(void){return (void*)1;}
int xSemaphoreTake(void *s,int wait){return 1;}
int xSemaphoreGive(void *s){return 1;}
void vTaskDelay(int n){clock_us+=n*1000;}
void vTaskDelete(void *p){}
esp_err_t esp_wifi_get_mac(int i,uint8_t *out){uint8_t mac[]={0x94,0xa9,0x90,0xcd,0x28,0xf4};memcpy(out,mac,6);return 0;}
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *out){out->rssi=-53;return 0;}
static esp_app_desc_t description={.version="0.11.2"};
const esp_app_desc_t *esp_app_get_description(void){return &description;}
int xTaskCreatePinnedToCore(void(*f)(void*),const char *n,unsigned stack,void *a,unsigned priority,void *h,int core){assert(priority==0&&core==0&&stack==8192);++workers;return pdPASS;}
esp_err_t esp_crt_bundle_attach(void *p){return 0;}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *cfg){assert(cfg->keep_alive_enable&&cfg->disable_auto_redirect);++inits;struct fake_client *h=calloc(1,sizeof(*h));assert(h);h->handler=cfg->event_handler;return h;}
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t h){++cleanups;free(h);return 0;}
esp_err_t esp_http_client_set_url(esp_http_client_handle_t h,const char *url){snprintf(h->url,sizeof(h->url),"%s",url);return 0;}
esp_err_t esp_http_client_set_timeout_ms(esp_http_client_handle_t h,int n){assert(n>=10000);return 0;}
esp_err_t esp_http_client_set_user_data(esp_http_client_handle_t h,void *p){h->user=p;client=*h;return 0;}
esp_err_t esp_http_client_set_header(esp_http_client_handle_t h,const char *key,const char *v){
 char *out=NULL;size_t cap=0;
 #define HEADER(k,f) if(!strcmp(key,k)){out=h->f;cap=sizeof(h->f);}
 HEADER("X-Ship-Device-Id",id);HEADER("X-Ship-Firmware",firmware);HEADER("X-Ship-Uptime",uptime);HEADER("X-Ship-Heap",heap);HEADER("X-Ship-Rssi",rssi);
 #undef HEADER
 if(out)snprintf(out,cap,"%s",v);
 return 0;
}
esp_err_t esp_http_client_perform(esp_http_client_handle_t h){
 ++attempts;clock_us+=handshake_us;h->status=next_code;if(fail)return ESP_FAIL;
 const char data[]="{}";esp_http_client_event_t e={.event_id=HTTP_EVENT_ON_DATA,.user_data=h->user,.data=(void*)data,.data_len=(int)payload_size};h->handler(&e);
 if(chunk_delay){clock_us+=chunk_delay;e.data_len=0;h->handler(&e);}return 0;
}
int esp_http_client_get_status_code(esp_http_client_handle_t h){return h->status;}
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t h){return complete;}
static bool request(void *out,size_t cap){size_t n;unsigned elapsed;int code;esp_err_t error;return fetch("/api/v1/snapshot?range_miles=30&limit=12",out,cap,&n,&elapsed,&code,&error);}
int main(int argc,char **argv){
 room_ships_start();assert(workers==2);assert(!strcmp(device_id,"thermo-94a990cd28f4"));char body[32];
 assert(request(body,sizeof(body)));assert(inits==1&&attempts==1&&cleanups==0&&client.user==NULL);
 assert(!strcmp(client.id,device_id)&&!strcmp(client.firmware,"0.11.2")&&!strcmp(client.rssi,"-53")&&atoi(client.heap)==120000);
 if(argc>1){FILE *f=fopen(argv[1],"wb");assert(f);fprintf(f,"{\"X-Ship-Device-Id\":\"%s\",\"X-Ship-Firmware\":\"%s\",\"X-Ship-Uptime\":\"%s\",\"X-Ship-Heap\":\"%s\",\"X-Ship-Rssi\":\"%s\"}",client.id,client.firmware,client.uptime,client.heap,client.rssi);fclose(f);}
 /* The former four-second-from-start deadline rejected this valid slow TLS request. */
 assert(clock_us>12000000);handshake_us=0;assert(request(body,sizeof(body)));assert(inits==1&&attempts==2);
 chunk_delay=5000000;assert(!request(body,sizeof(body))&&cleanups==1&&http_client==NULL);chunk_delay=0;
 assert(request(body,sizeof(body))&&inits==2);payload_size=2;assert(!request(body,1)&&http_client==NULL);
 assert(request(body,sizeof(body)));fail=true;assert(!request(body,sizeof(body))&&http_client==NULL);fail=false;
 next_code=302;assert(!request(body,sizeof(body)));next_code=200;complete=false;assert(!request(body,sizeof(body)));complete=true;
 updating=true;assert(!request(body,sizeof(body)));updating=false;
 unsigned before=inits;size_t n;unsigned elapsed;int code;esp_err_t error;
 assert(fetch("/api/v1/map.rgb565?range_miles=30&encoding=raw",body,sizeof(body),&n,&elapsed,&code,&error));assert(inits==before+1&&map_http_client);
 assert(request(body,sizeof(body)));assert(http_client&&http_client!=map_http_client);before=inits;
 assert(fetch("/api/v1/map.rgb565?range_miles=30&encoding=raw",body,sizeof(body),&n,&elapsed,&code,&error)&&inits==before);
 model.service=true;model.feed=true;model.received_at=clock_us;assert(room_ships_service_online()&&room_ships_online());
 model.feed=false;assert(room_ships_service_online()&&!room_ships_online()); /* Service reachable, feed down. */
 model.feed=true;radio=true;assert(!room_ships_online());radio=false;wifi=false;assert(!room_ships_service_online());wifi=true;
 clock_us+=SHIP_FRESH_US;assert(!room_ships_service_online());room_ships_t snapshot;room_ships_snapshot(&snapshot);assert(!snapshot.service);
 if(http_client)esp_http_client_cleanup(http_client);
 if(map_http_client)esp_http_client_cleanup(map_http_client);
 puts("PASS: ship heartbeat headers/identity, slow TLS acceptance, keepalive reuse, body deadlines/overflow/partial/redirect/OTA rejection, bounded freshness and separate AIS health");
}
