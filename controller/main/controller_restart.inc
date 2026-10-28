/* Caller holds the controller lock. Reboot is separate from Stop/Service:
 * release PWM without changing the persisted operating-mode selection. */
static void controller_restart_task(void *unused)
{
    (void)unused;
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
}
static bool schedule_controller_restart(const char *reason)
{
    if(restart_pending)return true;
    /* Task creation failure leaves the existing control state intact. */
    if(xTaskCreate(controller_restart_task,"controller_restart",2048,NULL,3,NULL)!=pdPASS)
        return false;
    restart_pending=true;
    release();
    event(reason);
    return true;
}
static esp_err_t restart_post(httpd_req_t *r)
{
    cJSON *j=body(r);
    bool confirmed=text_is(j,"confirm","restart_controller");
    cJSON_Delete(j);
    if(!confirmed)return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Confirm controller restart");
    xSemaphoreTake(lock,portMAX_DELAY);
    bool ok=schedule_controller_restart("Web restart requested; saved operating mode preserved");
    xSemaphoreGive(lock);
    if(!ok)return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"Could not schedule restart; please retry");
    return httpd_resp_sendstr(r,"{\"ok\":true,\"restarting\":true}");
}
