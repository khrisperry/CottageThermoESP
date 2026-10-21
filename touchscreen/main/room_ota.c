#include "room_ota.h"
#include "room_settings.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static atomic_bool busy;
static uint32_t boot_id;
bool room_ota_busy(void){return busy;}
static esp_err_t reply(httpd_req_t *r,int code,const char *message)
{
    if(code!=200)httpd_resp_set_status(r,code==403?"403 Forbidden":code==409?"409 Conflict":code==400?"400 Bad Request":"500 Internal Server Error");
    httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");httpd_resp_set_hdr(r,"Connection","close");
    cJSON *j=cJSON_CreateObject();cJSON_AddStringToObject(j,"message",message);char *s=cJSON_PrintUnformatted(j);cJSON_Delete(j);
    if(!s)return ESP_ERR_NO_MEM;
    esp_err_t err=httpd_resp_sendstr(r,s);free(s);return err;
}
cJSON *room_ota_status(void)
{
    const esp_partition_t *running=esp_ota_get_running_partition(),*next=esp_ota_get_next_update_partition(NULL);
    cJSON *j=cJSON_CreateObject();cJSON_AddStringToObject(j,"version",esp_app_get_description()->version);
    cJSON_AddStringToObject(j,"board","Waveshare ESP32-S3-Touch-LCD-4B");
    cJSON_AddStringToObject(j,"running_slot",running?running->label:"unknown");
    cJSON_AddNumberToObject(j,"slot_bytes",next?next->size:0);cJSON_AddBoolToObject(j,"uploading",busy);
    cJSON_AddNumberToObject(j,"boot_id",boot_id);return j;
}
static bool valid_header(const uint8_t *data,size_t size)
{
    if(size<sizeof(esp_image_header_t)+sizeof(esp_image_segment_header_t)+sizeof(esp_app_desc_t))return false;
    esp_image_header_t header;esp_app_desc_t descriptor;
    memcpy(&header,data,sizeof(header));memcpy(&descriptor,data+sizeof(header)+sizeof(esp_image_segment_header_t),sizeof(descriptor));
    return header.magic==ESP_IMAGE_HEADER_MAGIC&&header.chip_id==ESP_CHIP_ID_ESP32S3&&
        descriptor.magic_word==ESP_APP_DESC_MAGIC_WORD&&
        !memcmp(descriptor.project_name,"cottage_thermo_room_4b",sizeof("cottage_thermo_room_4b"));
}
static esp_err_t upload(httpd_req_t *r)
{
    if(!room_web_authorized(r,true))return reply(r,403,"Unlock settings on this device before uploading firmware.");
    char type[64];if(httpd_req_get_hdr_value_str(r,"Content-Type",type,sizeof(type))!=ESP_OK||strcmp(type,"application/octet-stream"))return reply(r,400,"Upload the application .bin as application/octet-stream.");
    const esp_partition_t *slot=esp_ota_get_next_update_partition(NULL),*running=esp_ota_get_running_partition();
    if(!slot||slot==running)return reply(r,409,"No OTA slot available. Install the OTA partition layout once over USB.");
    if(r->content_len<1024||(size_t)r->content_len>slot->size)return reply(r,400,"Firmware size does not fit the OTA slot.");
    bool expected=false;if(!atomic_compare_exchange_strong(&busy,&expected,true))return reply(r,409,"An upload is already in progress.");
    uint8_t *buffer=malloc(4096);if(!buffer){busy=false;return reply(r,500,"Upload buffer unavailable.");}
    size_t at=0,initial=0;esp_err_t err=ESP_OK;const char *error="Upload interrupted; previous firmware remains selected.";
    int64_t started=esp_timer_get_time();esp_ota_handle_t handle=0;bool begun=false;
    /* Collect a complete image header and app descriptor before touching flash. */
    while(initial<1024){int n=httpd_req_recv(r,(char*)buffer+initial,1024-initial);if(n<=0){err=ESP_FAIL;break;}initial+=n;}
    if(err==ESP_OK&&!valid_header(buffer,initial)){err=ESP_FAIL;error="Wrong image. Use cottage_thermo_room_4b.bin for the Waveshare 4B, not a controller, bootloader or merged image.";}
    if(err==ESP_OK){err=esp_ota_begin(slot,OTA_WITH_SEQUENTIAL_WRITES,&handle);begun=err==ESP_OK;}
    if(err==ESP_OK){err=esp_ota_write(handle,buffer,initial);at=initial;}
    while(err==ESP_OK&&at<(size_t)r->content_len){
        if(esp_timer_get_time()-started>180000000){err=ESP_ERR_TIMEOUT;break;}
        size_t wanted=(size_t)r->content_len-at;if(wanted>4096)wanted=4096;
        int n=httpd_req_recv(r,(char*)buffer,wanted);if(n<=0){err=ESP_FAIL;break;}
        err=esp_ota_write(handle,buffer,n);at+=n;vTaskDelay(pdMS_TO_TICKS(1));
    }
    free(buffer);
    if(err==ESP_OK){err=esp_ota_end(handle);begun=false;error="Image validation failed. Previous firmware remains selected.";}
    if(err==ESP_OK){err=esp_ota_set_boot_partition(slot);error="Could not select the new image. Previous firmware remains active.";}
    if(err!=ESP_OK){if(begun)esp_ota_abort(handle);busy=false;return reply(r,400,error);}
    /* Keep other large transfers paused until restart. The next boot confirms
     * itself only after LCD/touch/network/tasks initialize successfully. */
    bool scheduled=room_settings_reboot_later();if(!scheduled)busy=false;
    return reply(r,200,scheduled?"Firmware validated and selected. Display restarting; keep power connected.":"Firmware validated and selected. Restart the display manually to finish.");
}
void room_ota_confirm_boot(void)
{
    const esp_partition_t *running=esp_ota_get_running_partition();esp_ota_img_states_t state;
    if(running&&esp_ota_get_state_partition(running,&state)==ESP_OK&&state==ESP_OTA_IMG_PENDING_VERIFY)
        ESP_ERROR_CHECK(esp_ota_mark_app_valid_cancel_rollback());
}
void room_ota_register(httpd_handle_t server)
{
    boot_id=esp_random();const httpd_uri_t route={.uri="/api/firmware",.method=HTTP_POST,.handler=upload};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server,&route));
}
