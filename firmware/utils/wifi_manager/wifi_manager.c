/*
 * WiFi credentials manager — NVS read/write + softAP captive portal.
 *
 * The portal is intentionally minimal: a single HTML form posting to
 * /save. No CSS, no JS. The user only sees it once during first setup
 * (or after a credential reset).
 */

#include "wifi_manager.h"

#include <string.h>

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs.h"

static const char *TAG = "wifi_mgr";

#define NVS_NS  "wifi"
#define KEY_SSID "ssid"
#define KEY_PSK  "psk"

// ---------------------------------------------------------------------------
// NVS
// ---------------------------------------------------------------------------

bool wifi_manager_load_creds(char *ssid, size_t ssid_len,
                             char *psk,  size_t psk_len)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) {
        return false;
    }
    size_t s = ssid_len;
    size_t p = psk_len;
    esp_err_t e1 = nvs_get_str(h, KEY_SSID, ssid, &s);
    esp_err_t e2 = nvs_get_str(h, KEY_PSK,  psk,  &p);
    nvs_close(h);
    return (e1 == ESP_OK && e2 == ESP_OK);
}

bool wifi_manager_save_creds(const char *ssid, const char *psk)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        return false;
    }
    bool ok = (nvs_set_str(h, KEY_SSID, ssid) == ESP_OK) &&
              (nvs_set_str(h, KEY_PSK,  psk)  == ESP_OK) &&
              (nvs_commit(h) == ESP_OK);
    nvs_close(h);
    return ok;
}

// ---------------------------------------------------------------------------
// SoftAP captive portal
// ---------------------------------------------------------------------------

static const char *PORTAL_HTML =
    "<!doctype html><html><head><title>Elderly Bot setup</title></head>"
    "<body style=\"font-family:sans-serif;max-width:400px;margin:20px auto\">"
    "<h2>Elderly Companion Robot</h2>"
    "<p>Enter your home WiFi credentials. The robot will reboot and "
    "connect to your network.</p>"
    "<form method=\"POST\" action=\"/save\">"
    "<p><label>SSID:<br><input type=\"text\" name=\"ssid\" required style=\"width:100%\"></label></p>"
    "<p><label>Password:<br><input type=\"password\" name=\"psk\" required style=\"width:100%\"></label></p>"
    "<p><button type=\"submit\" style=\"width:100%;padding:10px\">Save & Reboot</button></p>"
    "</form></body></html>";

static esp_err_t portal_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, PORTAL_HTML, strlen(PORTAL_HTML));
}

static esp_err_t url_decode(char *dst, const char *src, size_t dst_len)
{
    size_t i = 0;
    while (*src && i + 1 < dst_len) {
        if (*src == '+') {
            dst[i++] = ' ';
            src++;
        } else if (*src == '%' && src[1] && src[2]) {
            char hex[3] = { src[1], src[2], 0 };
            dst[i++] = (char)strtol(hex, NULL, 16);
            src += 3;
        } else {
            dst[i++] = *src++;
        }
    }
    dst[i] = '\0';
    return ESP_OK;
}

static bool parse_form_field(const char *body, const char *key, char *out, size_t out_len)
{
    char pattern[32];
    snprintf(pattern, sizeof(pattern), "%s=", key);
    const char *p = strstr(body, pattern);
    if (!p) return false;
    p += strlen(pattern);
    const char *end = strchr(p, '&');
    size_t span = end ? (size_t)(end - p) : strlen(p);

    char raw[128];
    if (span >= sizeof(raw)) return false;
    memcpy(raw, p, span);
    raw[span] = '\0';
    url_decode(out, raw, out_len);
    return true;
}

static esp_err_t portal_save(httpd_req_t *req)
{
    char body[256];
    int total = 0;
    int r;
    while ((r = httpd_req_recv(req, body + total, sizeof(body) - 1 - total)) > 0) {
        total += r;
        if (total >= (int)sizeof(body) - 1) break;
    }
    body[total] = '\0';

    char ssid[33] = {0};
    char psk[65]  = {0};
    if (!parse_form_field(body, "ssid", ssid, sizeof(ssid)) ||
        !parse_form_field(body, "psk",  psk,  sizeof(psk))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing ssid/psk");
        return ESP_FAIL;
    }
    if (!wifi_manager_save_creds(ssid, psk)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "nvs save failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Saved creds for SSID '%s', rebooting in 2s", ssid);
    httpd_resp_send(req, "saved, rebooting...", 19);
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    return ESP_OK;
}

static void start_softap(void)
{
    esp_netif_create_default_wifi_ap();

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);

    wifi_config_t ap = {0};
    snprintf((char *)ap.ap.ssid, sizeof(ap.ap.ssid),
             "elderly-bot-%02X%02X", mac[4], mac[5]);
    ap.ap.ssid_len = strlen((char *)ap.ap.ssid);
    ap.ap.channel = 1;
    ap.ap.authmode = WIFI_AUTH_OPEN;
    ap.ap.max_connection = 2;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "softAP up: %s — connect and open http://192.168.4.1/", ap.ap.ssid);
}

void wifi_manager_start_portal(void)
{
    start_softap();

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(httpd_start(&server, &cfg));

    httpd_uri_t root = { .uri = "/",     .method = HTTP_GET,  .handler = portal_root };
    httpd_uri_t save = { .uri = "/save", .method = HTTP_POST, .handler = portal_save };
    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &save);
}
