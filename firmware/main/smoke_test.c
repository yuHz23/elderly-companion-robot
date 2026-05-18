/*
 * Smoke test — WiFi + camera + MJPEG server.
 *
 * Brings the robot up to the point where you can hit http://<ip>/stream
 * in a browser and see live camera frames. If this works end-to-end,
 * the hardware (rail, MCU, PSRAM, camera, antenna) is healthy and
 * subsequent phases can build on it.
 */

#include "smoke_test.h"

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
    char buf[256];
    int n = snprintf(buf, sizeof(buf),
        "{"
        "\"phase\":3,"
        "\"free_heap\":%u,"
        "\"min_free_heap\":%u,"
        "\"psram_size_mb\":%u,"
        "\"uptime_s\":%lld"
        "}",
        (unsigned)esp_get_free_heap_size(),
        (unsigned)esp_get_minimum_free_heap_size(),
        (unsigned)(esp_psram_get_size() / (1024 * 1024)),
        esp_timer_get_time() / 1000000);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, buf, n);
}

static esp_err_t root_handler(httpd_req_t *req)
{
    const char *html =
        "<!doctype html><html><head><title>Elderly Bot — Phase 3</title>"
        "<style>body{font-family:sans-serif;background:#111;color:#eee;padding:20px}"
        "img{max-width:100%;border-radius:8px}</style></head>"
        "<body><h2>Elderly Companion Robot — Phase 3 smoke test</h2>"
        "<p>Camera stream:</p>"
        "<img src=\"/stream\" alt=\"camera\"/>"
        "<p><a href=\"/status\" style=\"color:#0af\">JSON status</a></p>"
        "</body></html>";
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html, strlen(html));
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
    httpd_uri_t root = { .uri = "/",       .method = HTTP_GET, .handler = root_handler };
    httpd_uri_t st   = { .uri = "/status", .method = HTTP_GET, .handler = status_handler };
    httpd_uri_t str  = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler };
    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &st);
    httpd_register_uri_handler(server, &str);
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

    web_server_start();
    ESP_LOGI(TAG, "smoke test ready");
}
