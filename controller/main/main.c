/*
 * CottageThermoESP — minimal TS90MD servo BENCH tester
 *
 * ESP-IDF 6.x / ESP32-S3. NOT a furnace controller.
 * Never attach a test controller to an operating gas appliance.
 * Servo PWM starts DISABLED and commands are accepted only via HTTP POST.
 * This is not a position sensor: displayed µs = last COMMAND, not actual angle.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

static const char *TAG = "cottage_bench";

#define SERVO_MIN_US         1100
#define SERVO_MAX_US         1900
#define SERVO_PERIOD_US     20000
#define SERVO_FREQ_HZ          50
#define SERVO_PWM_BITS         13
#define SERVO_PWM_TIMER   LEDC_TIMER_0
#define SERVO_PWM_CHAN    LEDC_CHANNEL_0
#define SERVO_PWM_MODE    LEDC_LOW_SPEED_MODE

static bool s_ledc_timer_ready = false;
static bool s_pwm_active = false;
static int s_last_command_us = -1;

static uint32_t pulse_to_duty(int us)
{
    // 13-bit LEDC @ 50Hz; all math in integers, avoid overflow.
    return (uint32_t)(((uint64_t)us * (1U << SERVO_PWM_BITS)) / SERVO_PERIOD_US);
}

static esp_err_t servo_command_us(int us)
{
    if (us < SERVO_MIN_US || us > SERVO_MAX_US) {
        return ESP_ERR_INVALID_ARG;
    }

    // This is intentionally deferred until the FIRST explicit web command.
    if (!s_ledc_timer_ready) {
        ledc_timer_config_t t = {
            .speed_mode = SERVO_PWM_MODE,
            .duty_resolution = LEDC_TIMER_13_BIT,
            .timer_num = SERVO_PWM_TIMER,
            .freq_hz = SERVO_FREQ_HZ,
            .clk_cfg = LEDC_AUTO_CLK,
        };
        esp_err_t err = ledc_timer_config(&t);
        if (err != ESP_OK) return err;
        s_ledc_timer_ready = true;
    }

    // Reconfigure channel per command to also resume after ledc_stop().
    ledc_channel_config_t c = {
        .gpio_num = CONFIG_THERMO_SERVO_GPIO,
        .speed_mode = SERVO_PWM_MODE,
        .channel = SERVO_PWM_CHAN,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = SERVO_PWM_TIMER,
        .duty = pulse_to_duty(us),
        .hpoint = 0,
    };
    esp_err_t err = ledc_channel_config(&c);
    if (err != ESP_OK) return err;

    s_last_command_us = us;
    s_pwm_active = true;
    ESP_LOGI(TAG, "BENCH PWM COMMAND: %d us (not measured position)", us);
    return ESP_OK;
}

static void servo_release_pwm(void)
{
    if (s_ledc_timer_ready) {
        // Disable servo command signal; this is NOT a mechanical brake.
        ESP_ERROR_CHECK(ledc_stop(SERVO_PWM_MODE, SERVO_PWM_CHAN, 0));
    }
    s_pwm_active = false;
    ESP_LOGI(TAG, "BENCH PWM DISABLED: servo may be back-driven");
}

static const char s_html[] =
    "<!doctype html><html lang='en'><head>"
    "<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>CottageThermo Servo Bench</title>"
    "<style>body{font:16px system-ui,sans-serif;max-width:570px;margin:32px auto;padding:0 15px;"
    "background:#f7f7f9;color:#18212f}main{background:white;padding:24px;border-radius:14px;"
    "box-shadow:0 2px 16px #0001}button,input{font:inherit;padding:12px;border-radius:8px;margin:6px 4px 6px 0}"
    "input{width:120px;border:1px solid #999}button{cursor:pointer;border:1px solid #666}"
    ".warn{border-left:5px solid #d97706;background:#fff6e9;padding:13px}"
    "#status{white-space:pre-wrap;background:#edf1f6;padding:12px;border-radius:8px}</style></head>"
    "<body><main><h1>Servo bench tester</h1>"
    "<p class='warn'><strong>BENCH ONLY.</strong> Disconnect from the furnace. "
    "The servo may JUMP when PWM is first enabled. Remove the horn for initial tests. "
    "No position feedback or automatic motion on startup.</p>"
    "<p>Wi-Fi: <b>CottageThermoBench</b> (open, no password).</p>"
    "<p>50Hz PWM. Allowed range <b>1100-1900 microseconds</b>; start unloaded at 1500. "
    "Small steps only. This is NOT a degree-calibrated position.</p>"
    "<label for='us'>Command pulse (us):</label>"
    "<input id='us' type='number' min='1100' max='1900' step='10' value='1500'>"
    "<button onclick='sendPulse()'>Command PWM</button>"
    "<button onclick='stopPulse()'>Stop PWM</button>"
    "<button onclick='refreshStatus()'>Refresh status</button>"
    "<h3>Controller report</h3><div id='status'>Loading...</div>"
    "<p><small>Stopping PWM does NOT return the knob to zero or hold it in place. "
    "Reboot never restores last commanded PWM. Last command is not actual shaft angle.</small></p>"
    "<script>"
    "async function request(path,opts){try{const r=await fetch(path,opts);"
    "const t=await r.text();if(!r.ok)alert('HTTP '+r.status+': '+t);"
    "await refreshStatus();}catch(e){alert(e.message)}}"
    "function sendPulse(){const v=document.getElementById('us').value;"
    "request('/api/pulse',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},"
    "body:'us='+encodeURIComponent(v)})}"
    "function stopPulse(){request('/api/stop',{method:'POST'})}"
    "async function refreshStatus(){try{const r=await fetch('/api/status',{cache:'no-store'});"
    "document.getElementById('status').textContent=JSON.stringify(await r.json(),null,2)}"
    "catch(e){document.getElementById('status').textContent='Unavailable: '+e.message}}"
    "refreshStatus();</script></main></body></html>";

static esp_err_t root_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, s_html);
}

static esp_err_t status_get(httpd_req_t *req)
{
    char json[192];
    // No physical angle sensor is present. Never describe command as position.
    snprintf(json, sizeof(json),
             "{\"bench_only\":true,\"servo_gpio\":%d,\"pwm_active\":%s,\"last_command_us\":%d,"
             "\"servo_position_verified\":false,\"min_us\":%d,\"max_us\":%d}",
             CONFIG_THERMO_SERVO_GPIO, s_pwm_active ? "true" : "false", s_last_command_us,
             SERVO_MIN_US, SERVO_MAX_US);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, json);
}

static esp_err_t pulse_post(httpd_req_t *req)
{
    char body[32];
    if (req->content_len < 4 || req->content_len >= sizeof(body)) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid body");
    }

    size_t nread = 0;
    while (nread < req->content_len) {
        int n = httpd_req_recv(req, body + nread, req->content_len - nread);
        if (n <= 0) {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Incomplete request");
        }
        nread += (size_t)n;
    }
    body[nread] = '\0';
    if (strncmp(body, "us=", 3) != 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Expected us=NNNN");
    }

    char *end = NULL;
    long us = strtol(body + 3, &end, 10);
    if (end == body + 3 || *end != '\0' || us < SERVO_MIN_US || us > SERVO_MAX_US) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                   "Pulse out of 1100..1900 us bounds");
    }

    esp_err_t err = servo_command_us((int)us);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "PWM command failed: %s", esp_err_to_name(err));
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "PWM peripheral error");
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t stop_post(httpd_req_t *req)
{
    servo_release_pwm();
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true,\"pwm_active\":false}");
}

static void start_http(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    const httpd_uri_t root = {
        .uri = "/", .method = HTTP_GET, .handler = root_get,
    };
    const httpd_uri_t status = {
        .uri = "/api/status", .method = HTTP_GET, .handler = status_get,
    };
    const httpd_uri_t pulse = {
        .uri = "/api/pulse", .method = HTTP_POST, .handler = pulse_post,
    };
    const httpd_uri_t stop = {
        .uri = "/api/stop", .method = HTTP_POST, .handler = stop_post,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &status));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &pulse));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &stop));
}

static void start_softap(void)
{
    const char *ssid = CONFIG_THERMO_AP_SSID;

    // Open AP for short-range, supervised bench testing only.
    // Never require a password for the test device's captive-portal Wi-Fi.
    if (strlen(ssid) == 0 || strlen(ssid) > 31) {
        ESP_LOGE(TAG, "Invalid bench SoftAP SSID.");
        return;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));

    wifi_config_t ap = {0};
    strncpy((char *)ap.ap.ssid, ssid, sizeof(ap.ap.ssid) - 1);
    ap.ap.ssid_len = strlen(ssid);
    ap.ap.channel = 6;
    ap.ap.max_connection = 2;
    ap.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGW(TAG, "BENCH OPEN AP (NO PASSWORD): %s, web UI http://192.168.4.1", ssid);
    start_http();
}

void app_main(void)
{
    ESP_LOGW(TAG, "BENCH FIRMWARE ONLY: AtomS3 Lite G38/GPIO38 default; no furnace connection.");
    ESP_LOGI(TAG, "Servo PWM signal: GPIO%d; PWM disabled until web command", CONFIG_THERMO_SERVO_GPIO);
    // Output pin initialized LOW, not PWM. No command, sweep or homing on boot.
    gpio_config_t pin = {
        .pin_bit_mask = 1ULL << CONFIG_THERMO_SERVO_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&pin));
    ESP_ERROR_CHECK(gpio_set_level(CONFIG_THERMO_SERVO_GPIO, 0));

    ESP_ERROR_CHECK(nvs_flash_init());
    start_softap();
}
