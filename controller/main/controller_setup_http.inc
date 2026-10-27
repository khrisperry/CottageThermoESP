/* Included after the local controller helpers. Host tests compile this same
 * handler with fake PWM, NVS and HTTP boundaries. */
static cJSON *setup_json(void)
{
    cJSON *j=cJSON_CreateObject();
    cJSON_AddNumberToObject(j,"start_us",setup.start_us);
    cJSON_AddNumberToObject(j,"end_us",setup.end_us);
    const char *issue=controller_setup_ready(&setup);
    cJSON_AddBoolToObject(j,"ready",!issue);
    cJSON_AddStringToObject(j,"issue",issue?issue:"");
    cJSON *positions=cJSON_AddArrayToObject(j,"positions");
    for(int i=0;i<8;i++) {
        cJSON *p=cJSON_CreateObject();
        cJSON_AddNumberToObject(p,"index",i);
        cJSON_AddNumberToObject(p,"pulse_us",setup.pulse_us[i]);
        if(setup.angle_mask&(1U<<i))cJSON_AddNumberToObject(p,"angle_deg",setup.angle_deg[i]);
        if(i>=1 && i<=6 && (setup.temperature_mask&(1U<<(i-1))))cJSON_AddNumberToObject(p,"temp_f",setup.temperature_f[i-1]);
        cJSON_AddItemToArray(positions,p);
    }
    return j;
}
static bool save_setup(const controller_setup_t *next)
{
    if(!controller_setup_valid(next))return false;
    if(!memcmp(next,&setup,sizeof(setup)))return true;
    if(nvs_set_blob(store,"dial_setup_v1",next,sizeof(*next))!=ESP_OK || nvs_commit(store)!=ESP_OK)return false;
    setup=*next;th_release(&state);event("Dial calibration saved; automatic control remains stopped");return true;
}
/* One local workflow: stopped -> Test -> explicit Go. No remote target update
 * can arm PWM, calibrate, synchronize, or leave Test/Stopped mode. */
static esp_err_t setup_post(httpd_req_t *r)
{
    cJSON *j=body(r);
    if(!j)return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Send a valid setup request.");
    const char *error=NULL;double value=0;
    xSemaphoreTake(lock,portMAX_DELAY);
    uint64_t now=now_ms();
    bool testing=!go_active && state.config.mode==TH_HOLD && armed && now<armed_until;
    if(text_is(j,"action","test")) {
        if(!text_is(j,"confirm","unloaded_bench"))error="Confirm that this is an attended bench test.";
        else {release();state.config.mode=TH_HOLD;armed=true;armed_until=now+900000;event("Test mode: automatic control stopped; manual movement enabled for 15 minutes");}
    } else if(text_is(j,"action","pulse") || text_is(j,"action","jog")) {
        int us=0;
        if(!testing)error="Enter Test Mode before moving the servo.";
        else if(text_is(j,"action","jog")) {
            if(!number(j,"delta_us",&value) || floor(value)!=value || value==0 || fabs(value)>50)error="Choose a jog step between 1 and 50 microseconds.";
            else if(!pwm || last_pulse<500)error="Send a starting pulse before using jog.";
            else us=last_pulse+(int)value;
        } else if(!number(j,"us",&value) || floor(value)!=value || value<500 || value>2500)error="Starting pulse must be a whole number from 500 to 2500 microseconds.";
        else us=(int)value;
        if(!error && !controller_setup_contains(&setup,us))error="That movement is outside the saved travel limits.";
        if(!error) {
            if(pulse(us)!=ESP_OK){release();state.config.mode=TH_HOLD;error="Servo output failed. Enter Test Mode again.";}
            else {th_release(&state);event("Manual bench movement; automatic control remains stopped");}
        }
    } else if(text_is(j,"action","edit_limit")) {
        /* An incorrectly captured endpoint can otherwise trap the servo
         * inside its old boundary. Explicitly reopen one endpoint only,
         * retain the other endpoint and all numbered dial/temperature data.
         * Always stop PWM and disarm before the travel restriction changes. */
        bool start=text_is(j,"kind","start"),end=text_is(j,"kind","end");
        if(!start && !end)error="Choose Start or End to re-measure.";
        else if(!text_is(j,"confirm","recalibrate_limit"))
            error="Confirm that the unloaded fixture is attended before reopening a travel limit.";
        else if(!(start?setup.start_us:setup.end_us))
            error="That travel limit is already open. Move and capture it in Test Mode.";
        else {
            release();state.config.mode=TH_HOLD;
            controller_setup_t next=setup;
            if(start)next.start_us=0;else next.end_us=0;
            if(!save_setup(&next))
                error="Could not reopen the travel limit; previous calibration remains saved and movement is stopped.";
            else event(start?"Start limit reopened; retained dial settings; enter Test Mode to re-measure":
                             "End limit reopened; retained dial settings; enter Test Mode to re-measure");
        }
    } else if(text_is(j,"action","capture")) {
        if(!testing || !pwm || last_pulse<500)error="In Test Mode, move to the desired position before capturing it.";
        else {
            controller_setup_t next=setup;
            if(text_is(j,"kind","start"))next.start_us=last_pulse;
            else if(text_is(j,"kind","end"))next.end_us=last_pulse;
            else if(text_is(j,"kind","position") && number(j,"index",&value) && value>=0 && value<=7 && floor(value)==value) {
                if(!setup.start_us || !setup.end_us)error="Capture both travel limits before the dial markings.";
                else {
                    int i=(int)value;next.pulse_us[i]=last_pulse;
                    controller_encoder_sample_t e=controller_encoder_snapshot();
                    bool valid=e.online && now>=e.at_ms && now-e.at_ms<=2000 && e.magnet_detected && !e.too_weak && !e.too_strong;
                    next.angle_mask&=~(1U<<i);
                    if(valid){next.angle_mask|=1U<<i;next.angle_deg[i]=e.raw_angle*360.0f/4096.0f;}
                }
            } else error="Choose a travel limit or a dial marking to capture.";
            if(!error && !controller_setup_valid(&next))error="Limits must differ and contain all saved positions. Reset calibration to expand travel.";
            if(!error && !save_setup(&next))error="Could not save calibration. No new calibration was applied.";
        }
    } else if(text_is(j,"action","temperatures")) {
        if(go_active)error="Stop Go mode before changing estimated temperatures.";
        else {
            cJSON *temps=cJSON_GetObjectItemCaseSensitive(j,"values");controller_setup_t next=setup;
            if(!cJSON_IsArray(temps) || cJSON_GetArraySize(temps)!=6)error="Enter all six estimated temperatures.";
            else for(int i=0;i<6;i++) {
                cJSON *v=cJSON_GetArrayItem(temps,i);
                if(!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || v->valuedouble<40 || v->valuedouble>90 ||
                    (i && v->valuedouble<=next.temperature_f[i-1])){error="Temperatures must increase from dial 1 to 6 and stay between 40 and 90 F.";break;}
                next.temperature_f[i]=v->valuedouble;
            }
            if(!error){next.temperature_mask=63;if(!save_setup(&next))error="Could not save estimated temperatures.";}
        }
    } else if(text_is(j,"action","reset")) {
        if(!text_is(j,"confirm","reset_calibration"))error="Confirm the calibration reset.";
        else {
            release();state.config.mode=TH_HOLD;
            controller_setup_t next=controller_setup_defaults();
            if(!save_setup(&next))error="Could not reset saved calibration.";
            else event("Calibration reset; Test Mode must be entered again");
        }
    } else if(text_is(j,"action","go")) {
        error=controller_setup_ready(&setup);
        if(!error && !text_is(j,"confirm","start_bench"))error="Confirm physical automatic bench movement.";
        if(!error && (!number(j,"position",&value) || value<1 || value>6 || floor(value)!=value))error="Select the current dial number, 1 through 6.";
        if(!error && (!testing || !pwm || last_pulse!=setup.pulse_us[(int)value]))error="In Test Mode, move to the saved starting dial number before pressing Go.";
        th_state_t next=state;
        if(!error) {
            next.config.mode=TH_AUTO;next.config.schedule_enabled=false;next.simulated=false;
            next.config.dwell_ms=300000;next.config.max_level_step=0.25f;next.config.level_deadband=0.1f;
            if(!controller_setup_map(&setup,&next.config) || !th_sync(&next,(float)value))error="The saved calibration cannot be used. Check dial positions and estimated temperatures.";
        }
        if(!error) {
            th_action_t action=th_tick(&next,now);
            if(!action.move && strcmp(next.reason,"mapped_setting_reached")) {
                if(!strcmp(next.reason,"temperature_stale"))error="Waiting for a fresh temperature from the thermostat.";
                else if(!strcmp(next.reason,"target_outside_measured_map"))error="Target temperature is outside your estimated dial 1 to 6 temperatures.";
                else if(!strcmp(next.reason,"sensor_disagreement"))error="Room and secondary temperatures disagree by more than 5 F.";
                else error="Thermostat prerequisites are not satisfied.";
            }
        }
        if(!error) {
            state=next;go_active=true;armed_until=now+900000;
            event("Go: physical thermostat control started for an attended 15-minute bench session");
        }
    } else error="Unknown setup action.";
    xSemaphoreGive(lock);cJSON_Delete(j);
    if(error)return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,error);
    return httpd_resp_sendstr(r,"{\"ok\":true}");
}
