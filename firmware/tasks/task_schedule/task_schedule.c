/*
 * task_schedule — minute-precision daily cron.
 *
 * SNTP is started on init; until it syncs (typically a few seconds
 * after WiFi associates), the task naps and skips firing. Once synced,
 * the loop runs once per minute, fires any matching entries, then
 * waits for the next minute boundary.
 *
 * Default schedule is written to NVS on first boot if not present, so
 * a fresh robot has sensible behaviour without manual setup.
 */

#include "task_schedule.h"

#include <string.h>
#include <time.h>

#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

#include "task_behavior.h"

static const char *TAG = "schedule";

#define NVS_NS  "schedule"
#define TZ_OFFSET_S  (7 * 3600)   // Asia/Ho_Chi_Minh, no DST

static const sched_entry_t DEFAULTS[SCHED_ENTRY_COUNT] = {
    { 6,  0, "leave"  },
    { 8,  0, "patrol" },
    { 12, 0, "dock"   },
    { 14, 0, "leave"  },
    { 19, 0, "patrol" },
    { 22, 0, "dock"   },
};

// -- SNTP -----------------------------------------------------------------

void sntp_time_init(void)
{
    static bool started = false;
    if (started) return;
    started = true;

    setenv("TZ", "ICT-7", 1);   // Indochina Time, no DST
    tzset();

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    ESP_LOGI(TAG, "SNTP started — server pool.ntp.org, tz ICT-7");
}

bool sntp_time_synced(void)
{
    time_t now = 0;
    time(&now);
    return now > 1700000000;   // anything after late 2023 = real time
}

// -- NVS schedule entries -------------------------------------------------

static void key_for(uint8_t idx, char *out, size_t out_len)
{
    snprintf(out, out_len, "e%u", idx);
}

bool task_schedule_get(uint8_t idx, sched_entry_t *out)
{
    if (idx >= SCHED_ENTRY_COUNT || !out) return false;
    char key[8];
    key_for(idx, key, sizeof(key));

    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;
    size_t len = sizeof(*out);
    esp_err_t err = nvs_get_blob(h, key, out, &len);
    nvs_close(h);
    return err == ESP_OK && len == sizeof(*out);
}

bool task_schedule_set(uint8_t idx, const sched_entry_t *in)
{
    if (idx >= SCHED_ENTRY_COUNT || !in) return false;
    char key[8];
    key_for(idx, key, sizeof(key));

    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = nvs_set_blob(h, key, in, sizeof(*in));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK;
}

static void seed_defaults_if_empty(void)
{
    sched_entry_t e;
    if (task_schedule_get(0, &e)) return;        // already populated
    ESP_LOGI(TAG, "seeding default schedule (6 entries)");
    for (uint8_t i = 0; i < SCHED_ENTRY_COUNT; ++i) {
        task_schedule_set(i, &DEFAULTS[i]);
    }
}

// -- task -----------------------------------------------------------------

static void dispatch(const char *cmd)
{
    if      (!strcmp(cmd, "idle"))   task_behavior_request_idle();
    else if (!strcmp(cmd, "patrol")) task_behavior_request_patrol();
    else if (!strcmp(cmd, "dock"))   task_behavior_request_dock();
    else if (!strcmp(cmd, "leave"))  task_behavior_request_leave();
    else ESP_LOGW(TAG, "unknown cmd: %s", cmd);
}

static void schedule_task(void *arg)
{
    sntp_time_init();
    seed_defaults_if_empty();
    ESP_LOGI(TAG, "task started — waiting for SNTP sync");

    int last_fired_minute = -1;
    while (1) {
        if (!sntp_time_synced()) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        time_t now = 0;
        struct tm t;
        time(&now);
        localtime_r(&now, &t);

        int minute_of_day = t.tm_hour * 60 + t.tm_min;
        if (minute_of_day != last_fired_minute) {
            for (uint8_t i = 0; i < SCHED_ENTRY_COUNT; ++i) {
                sched_entry_t e;
                if (!task_schedule_get(i, &e)) continue;
                if (e.hour > 23 || e.minute > 59) continue;       // disabled
                if (e.hour == t.tm_hour && e.minute == t.tm_min) {
                    ESP_LOGI(TAG, "[%02d:%02d] firing entry %u: %s",
                             t.tm_hour, t.tm_min, i, e.cmd);
                    dispatch(e.cmd);
                }
            }
            last_fired_minute = minute_of_day;
        }

        // Sleep until just past the next minute boundary
        int secs_to_next = 60 - t.tm_sec + 1;
        vTaskDelay(pdMS_TO_TICKS(secs_to_next * 1000));
    }
}

void task_schedule_start(void)
{
    xTaskCreate(schedule_task, "schedule", 4096, NULL, 2, NULL);
}
