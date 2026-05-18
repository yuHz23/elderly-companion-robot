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
#include "task_audio.h"
#include "audio_i2s.h"
#include "task_sos.h"
#include "sim800l.h"
#include "task_dock.h"
#include "battery.h"
#include "ir_dock.h"

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
        "\"phase\":9,"
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

// ---------------------------------------------------------------------------
// Audio HTTP API
// ---------------------------------------------------------------------------

static esp_err_t audio_tone_handler(httpd_req_t *req)
{
    int freq = read_query_int(req, "freq", 1000);
    int ms   = read_query_int(req, "ms",   500);
    int vol  = read_query_int(req, "vol",  50);
    if (freq < 50 || freq > 8000 || ms < 50 || ms > 5000 || vol < 0 || vol > 100) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
            "freq 50..8000, ms 50..5000, vol 0..100");
        return ESP_FAIL;
    }
    if (!task_audio_request_tone(freq, ms, vol)) {
        httpd_resp_send_err(req, HTTPD_409_CONFLICT, "audio busy");
        return ESP_FAIL;
    }
    return reply_ok(req, NULL);
}

static esp_err_t audio_loopback_handler(httpd_req_t *req)
{
    int ms = read_query_int(req, "ms", 3000);
    if (ms < 200 || ms > 10000) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "ms 200..10000");
        return ESP_FAIL;
    }
    if (!task_audio_request_loopback(ms)) {
        httpd_resp_send_err(req, HTTPD_409_CONFLICT, "audio busy");
        return ESP_FAIL;
    }
    return reply_ok(req, NULL);
}

static esp_err_t audio_record_handler(httpd_req_t *req)
{
    int ms = read_query_int(req, "ms", 3000);
    if (ms < 200 || ms > 10000) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "ms 200..10000");
        return ESP_FAIL;
    }
    if (!task_audio_request_record(ms)) {
        httpd_resp_send_err(req, HTTPD_409_CONFLICT, "audio busy");
        return ESP_FAIL;
    }
    return reply_ok(req, NULL);
}

// Minimal RIFF/WAV header for 16-bit mono PCM
static void make_wav_header(uint8_t hdr[44], uint32_t sr, uint32_t data_bytes)
{
    uint32_t byte_rate   = sr * AUDIO_BYTES_PER_SAMPLE;
    uint32_t block_align = AUDIO_BYTES_PER_SAMPLE;
    uint32_t chunk_size  = 36 + data_bytes;
    memcpy(hdr + 0,  "RIFF", 4);
    hdr[4]  = chunk_size;       hdr[5]  = chunk_size >> 8;
    hdr[6]  = chunk_size >> 16; hdr[7]  = chunk_size >> 24;
    memcpy(hdr + 8,  "WAVE", 4);
    memcpy(hdr + 12, "fmt ", 4);
    hdr[16] = 16; hdr[17]=0; hdr[18]=0; hdr[19]=0;   // PCM chunk size
    hdr[20] = 1;  hdr[21]=0;                         // format = PCM
    hdr[22] = 1;  hdr[23]=0;                         // channels = 1
    hdr[24] = sr;            hdr[25] = sr >> 8;
    hdr[26] = sr >> 16;      hdr[27] = sr >> 24;
    hdr[28] = byte_rate;     hdr[29] = byte_rate >> 8;
    hdr[30] = byte_rate >> 16; hdr[31] = byte_rate >> 24;
    hdr[32] = block_align; hdr[33] = 0;
    hdr[34] = 16; hdr[35] = 0;                       // bits per sample
    memcpy(hdr + 36, "data", 4);
    hdr[40] = data_bytes;       hdr[41] = data_bytes >> 8;
    hdr[42] = data_bytes >> 16; hdr[43] = data_bytes >> 24;
}

// ---------------------------------------------------------------------------
// SOS / SIM800L HTTP API
// ---------------------------------------------------------------------------

static int read_query_str(httpd_req_t *req, const char *key, char *out, size_t out_len)
{
    size_t qlen = httpd_req_get_url_query_len(req) + 1;
    if (qlen <= 1) { out[0] = '\0'; return 0; }
    char *q = malloc(qlen);
    if (!q) { out[0] = '\0'; return 0; }
    int rc = 0;
    if (httpd_req_get_url_query_str(req, q, qlen) == ESP_OK) {
        if (httpd_query_key_value(q, key, out, out_len) == ESP_OK) rc = strlen(out);
        else out[0] = '\0';
    } else out[0] = '\0';
    free(q);
    return rc;
}

static esp_err_t sim800_status_handler(httpd_req_t *req)
{
    int csq = sim800_signal_quality();
    char phone1[SOS_PHONE_MAX_LEN] = {0};
    char phone2[SOS_PHONE_MAX_LEN] = {0};
    task_sos_get_phone1(phone1, sizeof(phone1));
    task_sos_get_phone2(phone2, sizeof(phone2));

    char buf[256];
    int n = snprintf(buf, sizeof(buf),
        "{\"state\":%d,\"rssi\":%d,\"registered\":%s,"
        "\"phone1\":\"%s\",\"phone2\":\"%s\","
        "\"trigger_count\":%lu}",
        (int)sim800_state(), csq,
        sim800_is_registered() ? "true" : "false",
        phone1, phone2,
        (unsigned long)task_sos_trigger_count());
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, buf, n);
}

static esp_err_t sos_config_handler(httpd_req_t *req)
{
    char p1[SOS_PHONE_MAX_LEN], p2[SOS_PHONE_MAX_LEN], txt[SOS_SMS_MAX_LEN];
    int n1 = read_query_str(req, "phone1", p1, sizeof(p1));
    int n2 = read_query_str(req, "phone2", p2, sizeof(p2));
    int nt = read_query_str(req, "sms",    txt, sizeof(txt));
    if (n1) task_sos_set_phone1(p1);
    if (n2) task_sos_set_phone2(p2);
    if (nt) task_sos_set_sms_text(txt);
    return reply_ok(req, NULL);
}

static esp_err_t sos_trigger_handler(httpd_req_t *req)
{
    task_sos_trigger();
    return reply_ok(req, "\"dispatched\":true");
}

static esp_err_t sim800_sms_handler(httpd_req_t *req)
{
    char phone[SOS_PHONE_MAX_LEN], text[SOS_SMS_MAX_LEN];
    int np = read_query_str(req, "to",   phone, sizeof(phone));
    int nt = read_query_str(req, "text", text,  sizeof(text));
    if (np == 0 || nt == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "need ?to=&text=");
        return ESP_FAIL;
    }
    bool ok = sim800_send_sms(phone, text);
    return reply_ok(req, ok ? "\"sent\":true" : "\"sent\":false");
}

static esp_err_t sim800_dial_handler(httpd_req_t *req)
{
    char phone[SOS_PHONE_MAX_LEN];
    if (read_query_str(req, "to", phone, sizeof(phone)) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "need ?to=");
        return ESP_FAIL;
    }
    bool ok = sim800_dial(phone);
    return reply_ok(req, ok ? "\"dialed\":true" : "\"dialed\":false");
}

static esp_err_t sim800_hangup_handler(httpd_req_t *req)
{
    sim800_hangup();
    return reply_ok(req, NULL);
}

// ---------------------------------------------------------------------------
// Dock HTTP API
// ---------------------------------------------------------------------------

static esp_err_t dock_state_handler(httpd_req_t *req)
{
    char buf[256];
    int n = snprintf(buf, sizeof(buf),
        "{\"state\":%d,\"state_name\":\"%s\","
        "\"battery\":{\"v\":%.2f,\"pct\":%u,\"charging\":%s,\"full\":%s,\"low\":%s},"
        "\"ir_beacon\":%u}",
        (int)task_dock_state(), task_dock_state_name(),
        battery_voltage(), battery_percent(),
        battery_is_charging() ? "true" : "false",
        battery_is_full()     ? "true" : "false",
        battery_is_low()      ? "true" : "false",
        ir_dock_signal_strength(50));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, buf, n);
}

static esp_err_t dock_start_handler(httpd_req_t *req)
{
    if (!task_dock_request_dock()) {
        httpd_resp_send_err(req, HTTPD_409_CONFLICT, "dock already in progress");
        return ESP_FAIL;
    }
    return reply_ok(req, NULL);
}

static esp_err_t dock_cancel_handler(httpd_req_t *req)
{
    task_dock_request_cancel();
    return reply_ok(req, NULL);
}

static esp_err_t dock_leave_handler(httpd_req_t *req)
{
    task_dock_request_leave();
    return reply_ok(req, NULL);
}

static esp_err_t audio_wav_handler(httpd_req_t *req)
{
    size_t n;
    const int16_t *buf = task_audio_last_recording(&n);
    if (!buf || n == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "no recording — POST /audio/record first");
        return ESP_FAIL;
    }
    uint32_t data_bytes = n * AUDIO_BYTES_PER_SAMPLE;
    uint8_t hdr[44];
    make_wav_header(hdr, AUDIO_SAMPLE_RATE_HZ, data_bytes);

    httpd_resp_set_type(req, "audio/wav");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"recording.wav\"");
    httpd_resp_send_chunk(req, (const char *)hdr, sizeof(hdr));
    httpd_resp_send_chunk(req, (const char *)buf, data_bytes);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
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
"    <label>Audio test</label>"
"    <div><button onclick='tone(440)'>Tone 440Hz</button>"
"    <button onclick='tone(1000)'>Tone 1kHz</button>"
"    <button onclick='loop()'>Loopback 3s</button></div>"
"    <div style='margin-top:8px'><button onclick='rec()'>Record 3s</button>"
"    <a href='/audio/recording.wav' style='color:#0af;margin-left:8px'>Download WAV</a></div>"
"  </div>"
"</div>"
"<div class=row>"
"  <div class=panel>"
"    <label>Dock & charging</label>"
"    <div class=st id=dk>loading...</div>"
"    <div style='margin-top:8px'>"
"      <button onclick='dkStart()'>Auto-dock</button>"
"      <button class=alt onclick='dkLeave()'>Leave dock</button>"
"      <button class=stop onclick='dkCancel()'>Cancel</button>"
"    </div>"
"  </div>"
"  <div class=panel>"
"    <label>SOS — emergency contacts</label>"
"    <label>Phone 1 (E.164 e.g. +84909...)</label>"
"    <input id=p1 type=tel placeholder='+84909123456' style='width:100%;padding:6px;background:#222;color:#eee;border:1px solid #444;border-radius:4px'>"
"    <label>Phone 2 (optional)</label>"
"    <input id=p2 type=tel placeholder='+84909987654' style='width:100%;padding:6px;background:#222;color:#eee;border:1px solid #444;border-radius:4px'>"
"    <div style='margin-top:8px'>"
"      <button onclick='saveSos()'>Save contacts</button>"
"      <button class=stop onclick='trigSos()'>TRIGGER SOS (test)</button>"
"    </div>"
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
"const tone=(hz)=>fetch('/audio/tone?freq='+hz+'&ms=800&vol=60');"
"const loop=()=>fetch('/audio/loopback?ms=3000');"
"const rec =()=>fetch('/audio/record?ms=3000');"
"function saveSos(){"
"  const p1=encodeURIComponent(document.getElementById('p1').value);"
"  const p2=encodeURIComponent(document.getElementById('p2').value);"
"  fetch('/sos/config?phone1='+p1+'&phone2='+p2);"
"}"
"function trigSos(){if(confirm('Send TEST SOS to configured contacts?'))fetch('/sos/trigger');}"
"const dkStart=()=>fetch('/dock/start');"
"const dkLeave=()=>fetch('/dock/leave');"
"const dkCancel=()=>fetch('/dock/cancel');"
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
"  try{const r=await fetch('/dock/state');const d=await r.json();"
"    document.getElementById('dk').textContent="
"      'state    '+d.state_name+'\\n'+"
"      'battery  '+d.battery.v.toFixed(2)+'V  '+d.battery.pct+'%'+(d.battery.charging?' [CHG]':'')+(d.battery.full?' [FULL]':'')+(d.battery.low?' [LOW!]':'')+'\\n'+"
"      'ir beam  '+d.ir_beacon+'/100';"
"  }catch(e){document.getElementById('dk').textContent='(no data)'}"
"  try{const r=await fetch('/sim800/status');const m=await r.json();"
"    const stMap={0:'OFF',1:'POWERING',2:'READY',3:'FAULT'};"
"    document.getElementById('st').textContent+="
"      '\\nsim800     '+stMap[m.state]+' rssi='+m.rssi+(m.registered?' registered':' NOT registered')+"
"      '\\nsos        '+m.phone1+(m.phone2?' / '+m.phone2:'')+' (triggers='+m.trigger_count+')';"
"    if(!document.getElementById('p1').value)document.getElementById('p1').value=m.phone1||'';"
"    if(!document.getElementById('p2').value)document.getElementById('p2').value=m.phone2||'';"
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
    httpd_uri_t aton = { .uri = "/audio/tone",      .method = HTTP_GET, .handler = audio_tone_handler };
    httpd_uri_t alop = { .uri = "/audio/loopback",  .method = HTTP_GET, .handler = audio_loopback_handler };
    httpd_uri_t arec = { .uri = "/audio/record",    .method = HTTP_GET, .handler = audio_record_handler };
    httpd_uri_t awav = { .uri = "/audio/recording.wav", .method = HTTP_GET, .handler = audio_wav_handler };
    httpd_uri_t mst  = { .uri = "/sim800/status",     .method = HTTP_GET, .handler = sim800_status_handler };
    httpd_uri_t msms = { .uri = "/sim800/sms",        .method = HTTP_GET, .handler = sim800_sms_handler };
    httpd_uri_t mdl  = { .uri = "/sim800/dial",       .method = HTTP_GET, .handler = sim800_dial_handler };
    httpd_uri_t mhup = { .uri = "/sim800/hangup",     .method = HTTP_GET, .handler = sim800_hangup_handler };
    httpd_uri_t scf  = { .uri = "/sos/config",        .method = HTTP_GET, .handler = sos_config_handler };
    httpd_uri_t strg = { .uri = "/sos/trigger",       .method = HTTP_GET, .handler = sos_trigger_handler };
    httpd_uri_t dks  = { .uri = "/dock/state",   .method = HTTP_GET, .handler = dock_state_handler };
    httpd_uri_t dkd  = { .uri = "/dock/start",   .method = HTTP_GET, .handler = dock_start_handler };
    httpd_uri_t dkc  = { .uri = "/dock/cancel",  .method = HTTP_GET, .handler = dock_cancel_handler };
    httpd_uri_t dkl  = { .uri = "/dock/leave",   .method = HTTP_GET, .handler = dock_leave_handler };
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
    httpd_register_uri_handler(server, &aton);
    httpd_register_uri_handler(server, &alop);
    httpd_register_uri_handler(server, &arec);
    httpd_register_uri_handler(server, &awav);
    httpd_register_uri_handler(server, &mst);
    httpd_register_uri_handler(server, &msms);
    httpd_register_uri_handler(server, &mdl);
    httpd_register_uri_handler(server, &mhup);
    httpd_register_uri_handler(server, &scf);
    httpd_register_uri_handler(server, &strg);
    httpd_register_uri_handler(server, &dks);
    httpd_register_uri_handler(server, &dkd);
    httpd_register_uri_handler(server, &dkc);
    httpd_register_uri_handler(server, &dkl);
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

    // Phase 7: audio I/O (I2S full-duplex). Foreground commands via HTTP.
    task_audio_start();

    // Phase 8: cellular SOS — subscribes to fall events from sensor task.
    // Power-on of SIM800L happens inside the task (slow; takes ~10-30s
    // for network registration). The HTTP server starts immediately
    // below regardless.
    task_sos_start();

    // Phase 9: auto-dock state machine. Battery + IR receiver init runs
    // inside; uses task_navigation for motion so motors stay watchdog-safe.
    task_dock_start();

    web_server_start();
    ESP_LOGI(TAG, "smoke test ready");
}
