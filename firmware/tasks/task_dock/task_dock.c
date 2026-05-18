/*
 * task_dock — finite state machine that drives the robot home to its
 * charger.
 *
 * State transitions:
 *
 *   IDLE      → SEARCH   on request_dock()
 *   SEARCH    → APPROACH when ir_dock_signal_strength() > THRESHOLD
 *   SEARCH    → FAULT    after SEARCH_TIMEOUT_S with no beacon
 *   APPROACH  → CONTACT  when sensor front distance < APPROACH_NEAR_CM
 *   APPROACH  → SEARCH   if beacon disappears mid-flight
 *   APPROACH  → FAULT    after APPROACH_TIMEOUT_S
 *   CONTACT   → CHARGING when battery_is_charging()
 *   CONTACT   → FAULT    after CONTACT_TIMEOUT_S
 *   CHARGING  → CHARGED  when battery_is_full()
 *   CHARGED   → IDLE     on request_leave()  (drives forward 30 cm first)
 *   any       → IDLE     on request_cancel()
 *
 * The task pushes 50 ms velocity commands to task_navigation. Because
 * the watchdog there expires after 500 ms with no command, simply
 * dropping into IDLE / FAULT (which stops sending) brakes the robot.
 */

#include "task_dock.h"

#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "battery.h"
#include "ir_dock.h"
#include "task_navigation.h"
#include "task_sensor_fusion.h"

static const char *TAG = "dock";

// Tunables (also documented in dock-spec.md §4.1)
#define TICK_MS                100
#define SEARCH_ROTATE_DPS      30
#define APPROACH_FWD_SPEED     30           // % of max
#define CONTACT_FWD_SPEED      15           // creep forward
#define LEAVE_FWD_SPEED        40
#define LEAVE_DURATION_MS      1500
#define BEACON_LOCK_THRESHOLD  60           // 0..100 from ir_dock_signal_strength
#define BEACON_LOSE_THRESHOLD  20
#define APPROACH_NEAR_CM       30
#define CONTACT_NEAR_CM        10
#define SEARCH_TIMEOUT_S       30
#define APPROACH_TIMEOUT_S     60
#define CONTACT_TIMEOUT_S      30

typedef enum { CMD_NONE, CMD_DOCK, CMD_LEAVE, CMD_CANCEL } cmd_t;

static dock_state_t s_state = DOCK_IDLE;
static cmd_t        s_pending_cmd = CMD_NONE;
static int64_t      s_state_entered_us = 0;

const char *task_dock_state_name(void)
{
    switch (s_state) {
    case DOCK_IDLE:     return "IDLE";
    case DOCK_SEARCH:   return "SEARCH";
    case DOCK_APPROACH: return "APPROACH";
    case DOCK_CONTACT:  return "CONTACT";
    case DOCK_CHARGING: return "CHARGING";
    case DOCK_CHARGED:  return "CHARGED";
    case DOCK_FAULT:    return "FAULT";
    }
    return "?";
}

dock_state_t task_dock_state(void) { return s_state; }

bool task_dock_request_dock(void)
{
    if (s_state != DOCK_IDLE && s_state != DOCK_CHARGED && s_state != DOCK_FAULT) {
        return false;
    }
    s_pending_cmd = CMD_DOCK;
    return true;
}

void task_dock_request_leave(void)   { s_pending_cmd = CMD_LEAVE; }
void task_dock_request_cancel(void)  { s_pending_cmd = CMD_CANCEL; }

// -------------------------------------------------------------------------

static void enter(dock_state_t st)
{
    if (st != s_state) {
        const char *from = task_dock_state_name();
        s_state = st;
        ESP_LOGI(TAG, "%s -> %s", from, task_dock_state_name());
        s_state_entered_us = esp_timer_get_time();
    }
}

static uint32_t elapsed_s(void)
{
    return (uint32_t)((esp_timer_get_time() - s_state_entered_us) / 1000000LL);
}

static void handle_pending(void)
{
    cmd_t c = s_pending_cmd;
    s_pending_cmd = CMD_NONE;
    if (c == CMD_NONE) return;

    if (c == CMD_CANCEL) {
        nav_emergency_stop();
        enter(DOCK_IDLE);
        return;
    }
    if (c == CMD_DOCK) {
        ESP_LOGI(TAG, "dock requested");
        enter(DOCK_SEARCH);
        return;
    }
    if (c == CMD_LEAVE) {
        if (s_state == DOCK_CHARGED || s_state == DOCK_CHARGING) {
            ESP_LOGI(TAG, "leave requested — driving forward");
            int64_t t0 = esp_timer_get_time();
            while ((esp_timer_get_time() - t0) / 1000 < LEAVE_DURATION_MS) {
                nav_set_velocity(LEAVE_FWD_SPEED, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            nav_emergency_stop();
            enter(DOCK_IDLE);
        }
        return;
    }
}

// -------------------------------------------------------------------------

static void dock_task(void *arg)
{
    if (!ir_dock_init()) {
        ESP_LOGE(TAG, "ir_dock init failed");
    }
    if (!battery_init()) {
        ESP_LOGE(TAG, "battery init failed");
    }
    enter(DOCK_IDLE);

    while (1) {
        handle_pending();

        switch (s_state) {
        case DOCK_IDLE:
            // wait — no motor commands sent, nav watchdog keeps motors off
            break;

        case DOCK_SEARCH: {
            nav_set_velocity(0, SEARCH_ROTATE_DPS);    // rotate CCW
            uint8_t strength = ir_dock_signal_strength(150);
            if (strength >= BEACON_LOCK_THRESHOLD) {
                ESP_LOGI(TAG, "beacon locked (strength=%d)", strength);
                nav_emergency_stop();
                enter(DOCK_APPROACH);
            } else if (elapsed_s() > SEARCH_TIMEOUT_S) {
                ESP_LOGW(TAG, "search timeout — no beacon found");
                nav_emergency_stop();
                enter(DOCK_FAULT);
            }
            break;
        }

        case DOCK_APPROACH: {
            sensor_state_t s;
            sensors_get_state(&s);
            uint8_t strength = ir_dock_signal_strength(100);

            if (s.dist_front_cm < APPROACH_NEAR_CM && s.dist_front_cm > 0) {
                nav_emergency_stop();
                enter(DOCK_CONTACT);
            } else if (strength < BEACON_LOSE_THRESHOLD) {
                ESP_LOGW(TAG, "beacon lost mid-approach — back to search");
                nav_emergency_stop();
                enter(DOCK_SEARCH);
            } else if (elapsed_s() > APPROACH_TIMEOUT_S) {
                ESP_LOGW(TAG, "approach timeout");
                nav_emergency_stop();
                enter(DOCK_FAULT);
            } else {
                // Mild heading correction: if beacon weakening, drift toward
                // higher signal. For now just drive forward — the obstacle
                // gate inside task_navigation already protects against walls.
                nav_set_velocity(APPROACH_FWD_SPEED, 0);
            }
            break;
        }

        case DOCK_CONTACT:
            if (battery_is_charging()) {
                ESP_LOGI(TAG, "charge contact made — vbat=%.2fV", battery_voltage());
                nav_emergency_stop();
                enter(DOCK_CHARGING);
            } else if (elapsed_s() > CONTACT_TIMEOUT_S) {
                ESP_LOGW(TAG, "contact timeout — no charge voltage seen");
                nav_emergency_stop();
                enter(DOCK_FAULT);
            } else {
                nav_set_velocity(CONTACT_FWD_SPEED, 0);   // creep
            }
            break;

        case DOCK_CHARGING:
            // motors are off — nav watchdog has expired by now
            if (battery_is_full()) {
                ESP_LOGI(TAG, "battery full — vbat=%.2fV", battery_voltage());
                enter(DOCK_CHARGED);
            } else if (!battery_is_charging()) {
                ESP_LOGW(TAG, "lost charge voltage — re-attempting contact");
                enter(DOCK_CONTACT);
            }
            break;

        case DOCK_CHARGED:
            // sit idle until user asks robot to leave
            break;

        case DOCK_FAULT:
            // sit and wait — user must /dock/cancel to clear
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }
}

void task_dock_start(void)
{
    xTaskCreate(dock_task, "dock", 4096, NULL, 3, NULL);
}
