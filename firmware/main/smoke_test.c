/*
 * Smoke test — WiFi + camera + MJPEG server.
 *
 * Brings the robot up to the point where you can hit http://<ip>/stream
 * in a browser and see live camera frames. If this works end-to-end,
 * the hardware (rail, MCU, PSRAM, camera, antenna) is healthy and
 * subsequent phases can build on it.
 */

#include "smoke_test.h"

#include <stdlib.h>
#include <string.h>

#include "esp_camera.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs.h"

#include "servo_pwm.h"
#include "task_ptz.h"
#include "task_navigation.h"
#include "task_sensor_fusion.h"
#include "mpu6050.h"

static const char *TAG = "smoke";

#define WIFI_CONNECT_TIMEOUT_MS 30000
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// Fallback creds — production loads from NVS via wifi_manager. These are
// only used for Phase 3 bring-up; replace with your network and reflash.
#ifndef WIFI_FALLBACK_SSID
#  define WIFI_FALLBACK_SSID "elderly-bot-setup"
#endif
#ifndef WIFI_FALLBACK_PSK
#  define WIFI_FALLBACK_PSK  "changeme123"
#endif

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_count = 0;
static const int WIFI_MAX_RETRY = 5;

// ---------------------------------------------------------------------------
// WiFi
// ---------------------------------------------------------------------------

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_count < WIFI_MAX_RETRY) {
            s_retry_count++;
            ESP_LOGW(TAG, "WiFi disconnected, retry %d/%d", s_retry_count, WIFI_MAX_RETRY);
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static bool load_wifi_creds(char *ssid, size_t ssid_len, char *psk, size_t psk_len)
{
    nvs_handle_t h;
    if (nvs_open("wifi", NVS_READONLY, &h) != ESP_OK) {
        return false;
    }
    size_t s = ssid_len, p = psk_len;
    esp_err_t e1 = nvs_get_str(h, "ssid", ssid, &s);
    esp_err_t e2 = nvs_get_str(h, "psk", psk, &p);
    nvs_close(h);
    return (e1 == ESP_OK && e2 == ESP_OK);
}

static bool wifi_sta_init(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    char ssid[33] = {0};
    char psk[65] = {0};
    if (!load_wifi_creds(ssid, sizeof(ssid), psk, sizeof(psk))) {
        ESP_LOGW(TAG, "No WiFi creds in NVS — using compile-time fallback. "
                       "Provision via wifi_manager later.");
        strncpy(ssid, WIFI_FALLBACK_SSID, sizeof(ssid) - 1);
        strncpy(psk,  WIFI_FALLBACK_PSK,  sizeof(psk) - 1);
    }
    ESP_LOGI(TAG, "Connecting to SSID: %s", ssid);

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, psk, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));
    return (bits & WIFI_CONNECTED_BIT) != 0;
}

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------

static bool camera_init(void)
{
    // ESP32-S3-CAM XiaoZhi variant — DVP pins are routed internally on the
    // module, so the firmware passes the module-specific pin numbers below.
    // If pinout differs on a future module, only this block changes.
    camera_config_t config = {
        .pin_pwdn  = -1,
        .pin_reset = -1,
        .pin_xclk  = 15,
        .pin_sccb_sda = 4,
        .pin_sccb_scl = 5,
        .pin_d7 = 16,
        .pin_d6 = 17,
        .pin_d5 = 18,
        .pin_d4 = 12,
        .pin_d3 = 10,
        .pin_d2 = 8,
        .pin_d1 = 9,
        .pin_d0 = 11,
        .pin_vsync = 6,
        .pin_href  = 7,
        .pin_pclk  = 13,
        .xclk_freq_hz = 20000000,
        .ledc_timer   = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size   = FRAMESIZE_VGA,   // 640x480 — good baseline
        .jpeg_quality = 12,              // 0-63, lower = better
        .fb_count     = 2,
        .fb_location  = CAMERA_FB_IN_PSRAM,
        .grab_mode    = CAMERA_GRAB_WHEN_EMPTY,
    };
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: 0x%x", err);
        return false;
    }
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        ESP_LOGI(TAG, "Camera sensor PID: 0x%02x", s->id.PID);
    }
    return true;
}

// ---------------------------------------------------------------------------
// HTTP handlers
// ---------------------------------------------------------------------------

#define BOUNDARY "frame-boundary"
static const char *STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" BOUNDARY;
static const char *STREAM_BOUNDARY = "\r\n--" BOUNDARY "\r\n";
static const char *STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static esp_err_t stream_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    char part_buf[64];
    while (1) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGW(TAG, "fb_get returned NULL");
            return ESP_FAIL;
        }
        size_t hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART, fb->len);
        if (httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY)) != ESP_OK ||
            httpd_resp_send_chunk(req, part_buf, hlen) != ESP_OK ||
            httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len) != ESP_OK) {
            esp_camera_fb_return(fb);
            return ESP_FAIL;
        }
        esp_camera_fb_return(fb);
    }
}

static esp_err_t status_handler(httpd_req_t *req)
{
    char buf[512];
    int n = snprintf(buf, sizeof(buf),
        "{"
        "\"phase\":6,"
        "\"free_heap\":%u,"
        "\"min_free_heap\":%u,"
        "\"psram_size_mb\":%u,"
        "\"uptime_s\":%lld,"
        "\"ptz\":{"
            "\"pan\":%u,\"tilt\":%u,"
            "\"pan_target\":%u,\"tilt_target\":%u,"
            "\"speed_dps\":%u"
        "},"
        "\"drive\":{"
            "\"linear\":%d,\"angular\":%d,"
            "\"active\":%s,"
            "\"left_trim\":%d,\"right_trim\":%d"
        "}"
        "}",
        (unsigned)esp_get_free_heap_size(),
        (unsigned)esp_get_minimum_free_heap_size(),
        (unsigned)(esp_psram_get_size() / (1024 * 1024)),
        esp_timer_get_time() / 1000000,
        ptz_get_pan_current(), ptz_get_tilt_current(),
        ptz_get_pan_target(),  ptz_get_tilt_target(),
        ptz_get_speed(),
        nav_get_linear(), nav_get_angular(),
        nav_is_active() ? "true" : "false",
        nav_get_left_trim(), nav_get_right_trim());
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, buf, n);
}

// ---------------------------------------------------------------------------
// PTZ HTTP API
// ---------------------------------------------------------------------------

static int read_query_int(httpd_req_t *req, const char *key, int defval)
{
    size_t qlen = httpd_req_get_url_query_len(req) + 1;
    if (qlen <= 1) return defval;
    char *q = malloc(qlen);
    if (!q) return defval;
    if (httpd_req_get_url_query_str(req, q, qlen) != ESP_OK) {
        free(q);
        return defval;
    }
    char val[16] = {0};
    int result = defval;
    if (httpd_query_key_value(q, key, val, sizeof(val)) == ESP_OK) {
        result = atoi(val);
    }
    free(q);
    return result;
}

static esp_err_t reply_ok(httpd_req_t *req, const char *extra_json)
{
    char buf[128];
    int n = snprintf(buf, sizeof(buf), "{\"ok\":true%s%s}",
                     extra_json ? "," : "",
                     extra_json ? extra_json : "");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, buf, n);
}

static esp_err_t ptz_pan_handler(httpd_req_t *req)
{
    int angle = read_query_int(req, "angle", -1);
    if (angle < 0 || angle > 180) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "angle must be 0..180");
        return ESP_FAIL;
    }
    ptz_set_pan_target((uint8_t)angle);
    char extra[32];
    snprintf(extra, sizeof(extra), "\"target\":%d", angle);
    return reply_ok(req, extra);
}

static esp_err_t ptz_tilt_handler(httpd_req_t *req)
{
    int angle = read_query_int(req, "angle", -1);
    if (angle < 0 || angle > 180) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "angle must be 0..180");
        return ESP_FAIL;
    }
    ptz_set_tilt_target((uint8_t)angle);
    char extra[32];
    snprintf(extra, sizeof(extra), "\"target\":%d", angle);
    return reply_ok(req, extra);
}

static esp_err_t ptz_center_handler(httpd_req_t *req)  { ptz_center(); return reply_ok(req, NULL); }
static esp_err_t ptz_park_handler(httpd_req_t *req)    { ptz_park();   return reply_ok(req, NULL); }

static esp_err_t ptz_speed_handler(httpd_req_t *req)
{
    int dps = read_query_int(req, "dps", -1);
    if (dps < 10 || dps > 200) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "dps must be 10..200");
        return ESP_FAIL;
    }
    ptz_set_speed((uint16_t)dps);
    return reply_ok(req, NULL);
}

static esp_err_t ptz_calibrate_handler(httpd_req_t *req)
{
    int pan_off  = read_query_int(req, "pan_offset",  0);
    int tilt_off = read_query_int(req, "tilt_offset", 0);
    if (pan_off < -10 || pan_off > 10 || tilt_off < -10 || tilt_off > 10) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "offset must be -10..10");
        return ESP_FAIL;
    }
    servo_set_offset(SERVO_PAN,  (int8_t)pan_off);
    servo_set_offset(SERVO_TILT, (int8_t)tilt_off);
    return reply_ok(req, NULL);
}

// ---------------------------------------------------------------------------
// Drive HTTP API
// ---------------------------------------------------------------------------

static esp_err_t drive_velocity_handler(httpd_req_t *req)
{
    int linear  = read_query_int(req, "linear",  0);
    int angular = read_query_int(req, "angular", 0);
    if (linear < -100 || linear > 100 || angular < -100 || angular > 100) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "linear/angular must be -100..100");
        return ESP_FAIL;
    }
    nav_set_velocity((int8_t)linear, (int8_t)angular);
    return reply_ok(req, NULL);
}

static esp_err_t drive_stop_handler(httpd_req_t *req)
{
    nav_emergency_stop();
    return reply_ok(req, NULL);
}

static esp_err_t drive_calibrate_handler(httpd_req_t *req)
{
    int lt = read_query_int(req, "left_trim",  0);
    int rt = read_query_int(req, "right_trim", 0);
    if (lt < -10 || lt > 10 || rt < -10 || rt > 10) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "trim must be -10..10");
        return ESP_FAIL;
    }
    nav_set_trim((int8_t)lt, (int8_t)rt);
    return reply_ok(req, NULL);
}

// ---------------------------------------------------------------------------
// Sensor HTTP API
// ---------------------------------------------------------------------------

static esp_err_t sensors_state_handler(httpd_req_t *req)
{
    sensor_state_t s;
    if (!sensors_get_state(&s)) {
        httpd_resp_send_err(req, HTTPD_503_SERVICE_UNAVAILABLE, "no sensor data yet");
        return ESP_FAIL;
    }
    char buf[512];
    int n = snprintf(buf, sizeof(buf),
        "{"
        "\"dist\":{\"front\":%u,\"back\":%u,\"left\":%u,\"right\":%u},"
        "\"imu\":{\"pitch\":%.1f,\"roll\":%.1f,\"tilt\":%.1f,\"accel_g\":%.2f},"
        "\"fall\":%s,"
        "\"age_ms\":%lld"
        "}",
        s.dist_front_cm, s.dist_back_cm, s.dist_left_cm, s.dist_right_cm,
        s.pitch_deg, s.roll_deg, s.tilt_deg, s.accel_mag_g,
        s.fall_detected ? "true" : "false",
        (esp_timer_get_time() - s.timestamp_us) / 1000);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, buf, n);
}

static esp_err_t sensors_calibrate_imu_handler(httpd_req_t *req)
{
    bool ok = mpu6050_calibrate_now();
    return reply_ok(req, ok ? "\"saved\":true" : "\"saved\":false");
}

static esp_err_t root_handler(httpd_req_t *req)
{
    static const char html[] =
"<!doctype html><html><head><title>Elderly Bot</title>"
"<meta name=viewport content='width=device-width,initial-scale=1'>"
"<style>"
"body{font-family:sans-serif;background:#111;color:#eee;padding:14px;margin:0;max-width:780px}"
"h2{margin:0 0 12px}"
"img{width:100%;border-radius:8px;display:block}"
".row{display:flex;gap:14px;margin-top:14px;flex-wrap:wrap}"
".panel{background:#1e1e1e;padding:14px;border-radius:8px;flex:1 1 280px}"
"label{display:block;margin:8px 0 4px;font-size:13px;color:#aaa}"
"input[type=range]{width:100%}"
"button{background:#2a6;border:0;color:#fff;padding:10px 14px;border-radius:6px;margin:4px 4px 0 0;cursor:pointer;font-size:14px}"
"button.alt{background:#444}"
"button.stop{background:#c33}"
".val{display:inline-block;width:36px;text-align:right;color:#0fa;font-family:monospace}"
".st{font-family:monospace;color:#8ad;font-size:13px;white-space:pre;line-height:1.4}"
"#jpad{position:relative;width:200px;height:200px;background:#222;border-radius:50%;margin:8px auto;touch-action:none;user-select:none}"
"#jdot{position:absolute;left:80px;top:80px;width:40px;height:40px;background:#2a6;border-radius:50%;transition:none}"
"</style></head><body>"
"<h2>Elderly Companion Robot</h2>"
"<img src='/stream' alt=camera>"
"<div class=row>"
"  <div class=panel>"
"    <label>Drive — drag joystick (release to stop)</label>"
"    <div id=jpad><div id=jdot></div></div>"
"    <div style='text-align:center'>"
"      <button class=stop onclick='dStop()'>Emergency Stop</button>"
"    </div>"
"  </div>"
"  <div class=panel>"
"    <label>Pan <span class=val id=pv>90</span>&deg;</label>"
"    <input type=range id=p min=10 max=170 value=90 oninput='setPan(this.value)'>"
"    <label>Tilt <span class=val id=tv>90</span>&deg;</label>"
"    <input type=range id=t min=30 max=150 value=90 oninput='setTilt(this.value)'>"
"    <label>PTZ speed <span class=val id=sv>50</span>&deg;/s</label>"
"    <input type=range id=s min=10 max=200 value=50 oninput='setSpd(this.value)'>"
"    <div><button onclick='go(\"/ptz/center\")'>Center</button>"
"    <button class=alt onclick='go(\"/ptz/park\")'>Park</button></div>"
"  </div>"
"</div>"
"<div class=row>"
"  <div class=panel>"
"    <label>Sensors</label>"
"    <div class=st id=sn>loading...</div>"
"    <div style='margin-top:8px'><button class=alt onclick='calIMU()'>Calibrate IMU (board flat)</button></div>"
"  </div>"
"  <div class=panel>"
"    <label>Status</label>"
"    <div class=st id=st>loading...</div>"
"  </div>"
"</div>"
"<script>"
"const f=u=>fetch(u);"
"const setPan=v=>{document.getElementById('pv').textContent=v;f('/ptz/pan?angle='+v)};"
"const setTilt=v=>{document.getElementById('tv').textContent=v;f('/ptz/tilt?angle='+v)};"
"const setSpd=v=>{document.getElementById('sv').textContent=v;f('/ptz/speed?dps='+v)};"
"const go=u=>f(u);"
"const dStop=()=>f('/drive/stop');"
"// 2D joystick — y axis = linear (forward = up = -y), x axis = angular (right = CW)"
"const pad=document.getElementById('jpad'),dot=document.getElementById('jdot');"
"let active=false,sendTimer=null,lastLin=0,lastAng=0;"
"function moveDot(cx,cy){"
"  const r=pad.getBoundingClientRect();"
"  const dx=cx-r.left-r.width/2, dy=cy-r.top-r.height/2;"
"  const mag=Math.min(Math.hypot(dx,dy),r.width/2-20);"
"  const ang=Math.atan2(dy,dx);"
"  const x=mag*Math.cos(ang), y=mag*Math.sin(ang);"
"  dot.style.left=(r.width/2-20+x)+'px';dot.style.top=(r.height/2-20+y)+'px';"
"  // Map to -100..+100. y is INVERTED (drag up = forward)."
"  lastLin=Math.round(-y/(r.height/2-20)*100);"
"  lastAng=Math.round(x/(r.width/2-20)*100);"
"}"
"function resetDot(){const r=pad.getBoundingClientRect();dot.style.left=(r.width/2-20)+'px';dot.style.top=(r.height/2-20)+'px';lastLin=0;lastAng=0;}"
"function sendCmd(){if(active)f('/drive/velocity?linear='+lastLin+'&angular='+lastAng);}"
"function start(e){active=true;e.preventDefault();sendTimer=setInterval(sendCmd,100);move(e);}"
"function move(e){if(!active)return;const t=e.touches?e.touches[0]:e;moveDot(t.clientX,t.clientY);}"
"function end(){active=false;clearInterval(sendTimer);resetDot();f('/drive/stop');}"
"pad.addEventListener('mousedown',start);pad.addEventListener('touchstart',start,{passive:false});"
"document.addEventListener('mousemove',move);document.addEventListener('touchmove',move,{passive:false});"
"document.addEventListener('mouseup',end);document.addEventListener('touchend',end);"
"const calIMU=()=>{if(confirm('Robot must be flat and still. Calibrate now?'))fetch('/sensors/calibrate_imu');};"
"// Status + sensor refresh"
"async function refresh(){"
"  try{const r=await fetch('/status');const j=await r.json();"
"  document.getElementById('st').textContent="
"    'pan        '+j.ptz.pan+' / '+j.ptz.pan_target+' deg\\n'+"
"    'tilt       '+j.ptz.tilt+' / '+j.ptz.tilt_target+' deg\\n'+"
"    'ptz speed  '+j.ptz.speed_dps+' deg/s\\n'+"
"    'drive      lin='+j.drive.linear+' ang='+j.drive.angular+' '+(j.drive.active?'ACTIVE':'idle')+'\\n'+"
"    'trim       L='+j.drive.left_trim+'%% R='+j.drive.right_trim+'%%\\n'+"
"    'uptime     '+j.uptime_s+' s\\n'+"
"    'free heap  '+(j.free_heap/1024|0)+' KB'"
"  }catch(e){}"
"  try{const r=await fetch('/sensors/state');const s=await r.json();"
"  const fmt=v=>v>=65535?'---':v+'cm';"
"  document.getElementById('sn').textContent="
"    '           '+fmt(s.dist.front)+'\\n'+"
"    '  front\\n'+"
"    '  '+fmt(s.dist.left)+'  <robot>  '+fmt(s.dist.right)+'\\n'+"
"    '  back\\n'+"
"    '           '+fmt(s.dist.back)+'\\n'+"
"    '\\n'+"
"    'pitch '+s.imu.pitch.toFixed(1)+'  roll '+s.imu.roll.toFixed(1)+'  tilt '+s.imu.tilt.toFixed(1)+'\\n'+"
"    'accel '+s.imu.accel_g.toFixed(2)+'g'+(s.fall?'  ⚠ FALL':'')+'\\n'+"
"    'age   '+s.age_ms+'ms'"
"  }catch(e){document.getElementById('sn').textContent='(no data)'}"
"}"
"setInterval(refresh,500);refresh();"
"</script></body></html>";
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html, sizeof(html) - 1);
}

static void web_server_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.max_open_sockets = 4;
    cfg.stack_size = 8192;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "HTTP server start failed");
        return;
    }
    httpd_uri_t root = { .uri = "/",            .method = HTTP_GET, .handler = root_handler };
    httpd_uri_t st   = { .uri = "/status",      .method = HTTP_GET, .handler = status_handler };
    httpd_uri_t str  = { .uri = "/stream",      .method = HTTP_GET, .handler = stream_handler };
    httpd_uri_t pan  = { .uri = "/ptz/pan",     .method = HTTP_GET, .handler = ptz_pan_handler };
    httpd_uri_t tilt = { .uri = "/ptz/tilt",    .method = HTTP_GET, .handler = ptz_tilt_handler };
    httpd_uri_t ctr  = { .uri = "/ptz/center",  .method = HTTP_GET, .handler = ptz_center_handler };
    httpd_uri_t prk  = { .uri = "/ptz/park",    .method = HTTP_GET, .handler = ptz_park_handler };
    httpd_uri_t spd  = { .uri = "/ptz/speed",   .method = HTTP_GET, .handler = ptz_speed_handler };
    httpd_uri_t cal  = { .uri = "/ptz/calibrate", .method = HTTP_GET, .handler = ptz_calibrate_handler };
    httpd_uri_t dvel = { .uri = "/drive/velocity",  .method = HTTP_GET, .handler = drive_velocity_handler };
    httpd_uri_t dstp = { .uri = "/drive/stop",      .method = HTTP_GET, .handler = drive_stop_handler };
    httpd_uri_t dcal = { .uri = "/drive/calibrate", .method = HTTP_GET, .handler = drive_calibrate_handler };
    httpd_uri_t sst  = { .uri = "/sensors/state",      .method = HTTP_GET, .handler = sensors_state_handler };
    httpd_uri_t scal = { .uri = "/sensors/calibrate_imu", .method = HTTP_GET, .handler = sensors_calibrate_imu_handler };
    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &st);
    httpd_register_uri_handler(server, &str);
    httpd_register_uri_handler(server, &pan);
    httpd_register_uri_handler(server, &tilt);
    httpd_register_uri_handler(server, &ctr);
    httpd_register_uri_handler(server, &prk);
    httpd_register_uri_handler(server, &spd);
    httpd_register_uri_handler(server, &cal);
    httpd_register_uri_handler(server, &dvel);
    httpd_register_uri_handler(server, &dstp);
    httpd_register_uri_handler(server, &dcal);
    httpd_register_uri_handler(server, &sst);
    httpd_register_uri_handler(server, &scal);
    ESP_LOGI(TAG, "HTTP server up — visit http://<ip>/");
}

// ---------------------------------------------------------------------------
// Entry
// ---------------------------------------------------------------------------

void smoke_test_run(void)
{
    ESP_LOGI(TAG, "smoke test start");

    if (!wifi_sta_init()) {
        ESP_LOGE(TAG, "WiFi failed — check SSID/PSK or signal. Camera-only mode.");
        // Continue anyway — still useful to know camera initializes.
    }

    if (!camera_init()) {
        ESP_LOGE(TAG, "Camera init failed — Phase 3 NOT passing");
        return;
    }

    // Phase 4: spawn PTZ task (servo_init runs inside)
    task_ptz_start();

    // Phase 6: sensor fusion BEFORE nav so the obstacle gate has data
    task_sensor_fusion_start();

    // Phase 5: spawn navigation task (motor_init runs inside).
    // Reads sensor queue published by sensor_fusion above.
    task_navigation_start();

    web_server_start();
    ESP_LOGI(TAG, "smoke test ready");
}
