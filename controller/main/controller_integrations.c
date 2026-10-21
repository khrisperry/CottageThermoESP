#include "controller_integrations.h"
#include "sdkconfig.h"
#include "nvs.h"
#include <stdlib.h>
static cottage_integration_t current;
const cottage_integration_t *controller_integrations_get(void){return &current;}
void controller_integrations_init(void)
{
    snprintf(current.prefix,sizeof(current.prefix),"homeassistant");snprintf(current.name,sizeof(current.name),"Cottage thermostat");
#if CONFIG_COTTAGE_ESPNOW
    current.radio_enabled=true;snprintf(current.peer,sizeof(current.peer),"%s",CONFIG_COTTAGE_ESPNOW_PEER);snprintf(current.key,sizeof(current.key),"%s",CONFIG_COTTAGE_ESPNOW_KEY);
#endif
    nvs_handle_t nvs;size_t size=sizeof(current);cottage_integration_t *saved=calloc(1,sizeof(current));
    if(nvs_open("integrations",NVS_READONLY,&nvs)==ESP_OK){
        if(saved&&nvs_get_blob(nvs,"profile",saved,&size)==ESP_OK&&size==sizeof(current)&&cottage_integration_terminated(saved))current=*saved;
        nvs_close(nvs);
    }
    if(saved){memset(saved,0,sizeof(*saved));free(saved);}
    /* Legacy records remain readable, but this device never uses MQTT. */
    current.mqtt_enabled=false;
    memset(current.broker,0,sizeof(current.broker));
    memset(current.username,0,sizeof(current.username));
    memset(current.password,0,sizeof(current.password));
}
bool controller_integrations_save(cJSON *j,bool *changed)
{
    *changed=false;cottage_integration_t *next=malloc(sizeof(*next));if(!next)return false;*next=current;
    cJSON *mqtt=cJSON_GetObjectItemCaseSensitive(j,"mqtt"),*radio=cJSON_GetObjectItemCaseSensitive(j,"radio");
    bool ok=cJSON_IsObject(j)&&(mqtt||radio)&&(!mqtt||cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(mqtt,"enabled")))&&(!mqtt||cottage_integration_patch(next,mqtt,false))&&(!radio||cottage_integration_patch(next,radio,true));
    /* Compare against persisted desired settings too: an ambiguous network
     * outcome must not schedule repeated restarts for an identical profile. */
    nvs_handle_t nvs;
    if(ok&&nvs_open("integrations",NVS_READWRITE,&nvs)==ESP_OK){
        cottage_integration_t *saved=malloc(sizeof(*saved));size_t size=sizeof(*saved);
        bool same=saved&&nvs_get_blob(nvs,"profile",saved,&size)==ESP_OK&&size==sizeof(*saved)&&!memcmp(saved,next,sizeof(*next));
        if(saved){memset(saved,0,sizeof(*saved));free(saved);}
        if(!same)ok=nvs_set_blob(nvs,"profile",next,sizeof(*next))==ESP_OK&&nvs_commit(nvs)==ESP_OK;
        *changed=ok&&memcmp(next,&current,sizeof(*next))!=0;
        nvs_close(nvs);
    }else ok=false;
    memset(next,0,sizeof(*next));free(next);return ok;
}
