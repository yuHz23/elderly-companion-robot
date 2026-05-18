/*
 * task_sos — fall-event → cellular SOS bridge.
 *
 * On startup the task powers on SIM800L (slow — up to 30s for network
 * registration) then sits waiting for either:
 *   - EVT_FALL_DETECTED from the sensor event group
 *   - a manual task_sos_trigger() call (via FreeRTOS task notification)
 *
 * When either fires, it sends SMS + dials the configured contacts.
 * Cooldown is enforced by task_sensor_fusion (won't retrigger fall for
 * 30s), so we don't need a separate guard here.
 */

#include "task_sos.h"

#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

#include "sim800l.h"
#include "task_sensor_fusion.h"

static const char *TAG = "sos";

#define NVS_NS               "sos"
#define DEFAULT_SMS_TEXT     "[ELDERLY ROBOT SOS] Phat hien NGUOI NHA BI TE NGA. Vui long lien lac ngay."

static TaskHandle_t s_task = NULL;
static uint32_t     s_trigger_count = 0;
static int64_t      s_last_trigger_us = 0;

// -- NVS helpers ----------------------------------------------------------

static bool nvs_get_string(const char *key, char *out, size_t out_len)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;
    size_t len = out_len;
    esp_err_t err = nvs_get_str(h, key, out, &len);
    nvs_close(h);
    return err == ESP_OK && len > 1;        // empty string treated as unset
}

static bool nvs_set_string(const char *key, const char *value)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = nvs_set_str(h, key, value);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK;
}

// -- public ---------------------------------------------------------------

bool task_sos_set_phone1(const char *p)    { return nvs_set_string("phone1", p); }
bool task_sos_set_phone2(const char *p)    { return nvs_set_string("phone2", p); }
bool task_sos_set_sms_text(const char *t)  { return nvs_set_string("sms_text", t); }

bool task_sos_get_phone1(char *o, size_t l)   { return nvs_get_string("phone1", o, l); }
bool task_sos_get_phone2(char *o, size_t l)   { return nvs_get_string("phone2", o, l); }
bool task_sos_get_sms_text(char *o, size_t l) { return nvs_get_string("sms_text", o, l); }

uint32_t task_sos_trigger_count(void)   { return s_trigger_count; }
int64_t  task_sos_last_trigger_us(void) { return s_last_trigger_us; }

void task_sos_trigger(void)
{
    if (s_task) xTaskNotifyGive(s_task);
}

// -- SOS sequence ---------------------------------------------------------

static void dispatch_sos(void)
{
    s_trigger_count++;
    s_last_trigger_us = esp_timer_get_time();

    if (sim800_state() != SIM800_READY) {
        ESP_LOGW(TAG, "SIM800 not ready — retrying power-on");
        if (!sim800_power_on()) {
            ESP_LOGE(TAG, "SIM800 power-on failed — SOS NOT sent");
            return;
        }
    }

    char phone1[SOS_PHONE_MAX_LEN] = {0};
    char phone2[SOS_PHONE_MAX_LEN] = {0};
    char sms_text[SOS_SMS_MAX_LEN] = {0};

    bool has_p1 = task_sos_get_phone1(phone1, sizeof(phone1));
    bool has_p2 = task_sos_get_phone2(phone2, sizeof(phone2));
    if (!task_sos_get_sms_text(sms_text, sizeof(sms_text))) {
        strncpy(sms_text, DEFAULT_SMS_TEXT, sizeof(sms_text) - 1);
    }

    if (!has_p1 && !has_p2) {
        ESP_LOGW(TAG, "no contacts configured — set /sos/config first");
        return;
    }

    ESP_LOGE(TAG, "*** SOS DISPATCH ***");

    if (has_p1) {
        ESP_LOGI(TAG, "SMS → %s", phone1);
        if (!sim800_send_sms(phone1, sms_text)) {
            ESP_LOGW(TAG, "SMS p1 failed");
        }
    }
    if (has_p2) {
        ESP_LOGI(TAG, "SMS → %s", phone2);
        if (!sim800_send_sms(phone2, sms_text)) {
            ESP_LOGW(TAG, "SMS p2 failed");
        }
    }
    if (has_p1) {
        ESP_LOGI(TAG, "dialing → %s", phone1);
        if (!sim800_dial(phone1)) {
            ESP_LOGW(TAG, "dial failed");
        }
        // Let it ring up to 30s, then hang up
        vTaskDelay(pdMS_TO_TICKS(30000));
        sim800_hangup();
    }
    ESP_LOGI(TAG, "SOS dispatch complete");
}

// -- task -----------------------------------------------------------------

static void sos_task(void *arg)
{
    if (!sim800_init()) {
        ESP_LOGE(TAG, "UART init failed — task exiting");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "powering on SIM800L (this can take 10-30s)");
    sim800_power_on();    // best-effort; will retry on demand

    EventGroupHandle_t evt = sensors_event_group();
    if (!evt) {
        ESP_LOGE(TAG, "sensors event group missing — fall trigger disabled");
    }

    while (1) {
        // Wait for either: fall event bit set, or manual notification.
        // Use small timeout so we can poll both sources.
        EventBits_t bits = 0;
        if (evt) {
            bits = xEventGroupWaitBits(evt, EVT_FALL_DETECTED,
                                       pdFALSE,   // do not clear — sensor task manages clear via FALL_CLEAR
                                       pdFALSE, pdMS_TO_TICKS(200));
        } else {
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        bool fall = (bits & EVT_FALL_DETECTED) != 0;
        bool manual = ulTaskNotifyTake(pdTRUE, 0) > 0;

        if (fall || manual) {
            ESP_LOGW(TAG, "trigger source: %s%s",
                     fall   ? "FALL " : "",
                     manual ? "MANUAL" : "");
            dispatch_sos();
            // After SOS for a fall, the sensor task's 30s cooldown blocks
            // re-trigger. Wait a few seconds anyway to avoid spamming if
            // fall bit is still asserted.
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
}

void task_sos_start(void)
{
    xTaskCreate(sos_task, "sos", 6144, NULL, 3, &s_task);
}
