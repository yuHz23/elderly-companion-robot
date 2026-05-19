/*
 * task_mqtt — active state publisher + edge-triggered events.
 *
 * State publish (1 Hz, QoS 0): full JSON snapshot. HA's `value_template`
 * cherry-picks fields from this — see docs/deploy/home-assistant.md.
 *
 * Event publish (QoS 1): only when something edges. We keep last-seen
 * flags so we publish "battery low" exactly once per low->high cycle,
 * not every tick.
 */

#include "task_mqtt.h"

#include <stdio.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "battery.h"
#include "robot_mqtt.h"
#include "task_behavior.h"
#include "task_dock.h"
#include "task_navigation.h"
#include "task_ptz.h"
#include "task_sensor_fusion.h"
#include "sim800l.h"

static const char *TAG = "task_mqtt";

#define PUBLISH_PERIOD_MS  1000

static void publish_state_snapshot(void)
{
    sensor_state_t s = {0};
    bool have_sensor = sensors_get_state(&s);

    char buf[768];
    int n = snprintf(buf, sizeof(buf),
        "{"
        "\"uptime_s\":%lld,"
        "\"behavior\":\"%s\","
        "\"dock\":\"%s\","
        "\"sim800\":%d,"
        "\"battery\":{\"v\":%.2f,\"pct\":%u,\"charging\":%s,\"low\":%s,\"full\":%s},"
        "\"ptz\":{\"pan\":%u,\"tilt\":%u},"
        "\"drive\":{\"linear\":%d,\"angular\":%d,\"active\":%s},"
        "\"sensors\":{"
            "\"front\":%u,\"back\":%u,\"left\":%u,\"right\":%u,"
            "\"tilt\":%.1f,\"fall\":%s,\"fresh\":%s"
        "}"
        "}",
        esp_timer_get_time() / 1000000LL,
        task_behavior_state_name(),
        task_dock_state_name(),
        (int)sim800_state(),
        battery_voltage(), battery_percent(),
        battery_is_charging() ? "true" : "false",
        battery_is_low()      ? "true" : "false",
        battery_is_full()     ? "true" : "false",
        ptz_get_pan_current(), ptz_get_tilt_current(),
        nav_get_linear(), nav_get_angular(),
        nav_is_active() ? "true" : "false",
        have_sensor ? s.dist_front_cm : 0,
        have_sensor ? s.dist_back_cm  : 0,
        have_sensor ? s.dist_left_cm  : 0,
        have_sensor ? s.dist_right_cm : 0,
        have_sensor ? s.tilt_deg : 0.0f,
        have_sensor && s.fall_detected ? "true" : "false",
        have_sensor ? "true" : "false");
    mqtt_publish_state_json(buf);
    (void)n;
}

static void mqtt_task(void *arg)
{
    ESP_LOGI(TAG, "publisher task started — 1 Hz");

    EventGroupHandle_t evt = sensors_event_group();

    // Last-seen edges so we don't double-publish.
    bool was_fall = false;
    bool was_low  = false;
    bool was_charging = false;
    dock_state_t  prev_dock  = task_dock_state();
    behavior_state_t prev_bhv = task_behavior_state();

    TickType_t last_wake = xTaskGetTickCount();
    while (1) {
        publish_state_snapshot();

        // Edge events
        bool fall = (evt && (xEventGroupGetBits(evt) & EVT_FALL_DETECTED)) != 0;
        if (fall != was_fall) {
            mqtt_publish_event("fall", fall ? "1" : "0");
            was_fall = fall;
        }

        bool low = battery_is_low();
        if (low != was_low) {
            mqtt_publish_event("battery", low ? "low" : "ok");
            was_low = low;
        }
        bool charging = battery_is_charging();
        if (charging != was_charging) {
            mqtt_publish_event("charge", charging ? "started" : "stopped");
            was_charging = charging;
        }

        dock_state_t dock_now = task_dock_state();
        if (dock_now != prev_dock) {
            mqtt_publish_event("dock", task_dock_state_name());
            prev_dock = dock_now;
        }
        behavior_state_t bhv_now = task_behavior_state();
        if (bhv_now != prev_bhv) {
            mqtt_publish_event("behavior", task_behavior_state_name());
            prev_bhv = bhv_now;
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(PUBLISH_PERIOD_MS));
    }
}

void task_mqtt_start(void)
{
    // Only spawn if robot_mqtt actually connected to a broker. We rely on
    // robot_mqtt_start() having been called first; mqtt_publish_* are
    // no-ops until connection succeeds, so spawning unconditionally is
    // safe — we just waste a little CPU on JSON formatting until ready.
    xTaskCreate(mqtt_task, "mqtt", 6144, NULL, 2, NULL);
}
