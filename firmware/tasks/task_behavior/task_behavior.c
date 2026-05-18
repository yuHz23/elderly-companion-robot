/*
 * task_behavior — top-of-stack FSM. Reads everyone, commands everyone.
 *
 * Decision frequency: 5Hz (200ms tick). Higher than this overspends CPU
 * since every nav setpoint is rate-limited by task_navigation's own
 * 50Hz loop and watchdog. Lower than this makes fall preemption sluggish.
 *
 * SOS preemption is handled by also waking on the fall event bit (with
 * a short timeout so we still tick periodically).
 */

#include "task_behavior.h"

#include <stdlib.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "battery.h"
#include "task_dock.h"
#include "task_navigation.h"
#include "task_sensor_fusion.h"

static const char *TAG = "behavior";

#define TICK_MS                  200
#define PATROL_CMD_INTERVAL_MS  3000
#define SOS_RECOVERY_HOLD_S       30

typedef enum { CMD_NONE, CMD_IDLE, CMD_PATROL, CMD_DOCK, CMD_LEAVE } cmd_t;

static behavior_state_t s_state = BHV_IDLE;
static behavior_state_t s_pre_sos_state = BHV_IDLE;
static cmd_t            s_pending = CMD_NONE;
static int64_t          s_state_entered_us = 0;
static int64_t          s_last_patrol_cmd_us = 0;

const char *task_behavior_state_name(void)
{
    switch (s_state) {
    case BHV_IDLE:        return "IDLE";
    case BHV_PATROL:      return "PATROL";
    case BHV_RETURN_HOME: return "RETURN_HOME";
    case BHV_DOCKED:      return "DOCKED";
    case BHV_SOS_ACTIVE:  return "SOS_ACTIVE";
    case BHV_FAULT:       return "FAULT";
    }
    return "?";
}

behavior_state_t task_behavior_state(void) { return s_state; }

bool task_behavior_request_idle(void)   { s_pending = CMD_IDLE;   return true; }
bool task_behavior_request_patrol(void) { s_pending = CMD_PATROL; return true; }
bool task_behavior_request_dock(void)   { s_pending = CMD_DOCK;   return true; }
bool task_behavior_request_leave(void)  { s_pending = CMD_LEAVE;  return true; }

// -- helpers --------------------------------------------------------------

static void enter(behavior_state_t st)
{
    if (st == s_state) return;
    const char *from = task_behavior_state_name();
    s_state = st;
    ESP_LOGI(TAG, "%s -> %s", from, task_behavior_state_name());
    s_state_entered_us = esp_timer_get_time();
}

static void handle_pending(void)
{
    cmd_t c = s_pending;
    s_pending = CMD_NONE;
    if (c == CMD_NONE) return;
    if (s_state == BHV_SOS_ACTIVE) {
        ESP_LOGW(TAG, "ignoring user cmd while SOS_ACTIVE");
        return;
    }
    switch (c) {
    case CMD_IDLE:
        nav_emergency_stop();
        task_dock_request_cancel();
        enter(BHV_IDLE);
        break;
    case CMD_PATROL:
        if (battery_is_low()) {
            ESP_LOGW(TAG, "PATROL refused — battery low, going home");
            enter(BHV_RETURN_HOME);
            task_dock_request_dock();
        } else {
            enter(BHV_PATROL);
        }
        break;
    case CMD_DOCK:
        enter(BHV_RETURN_HOME);
        task_dock_request_dock();
        break;
    case CMD_LEAVE:
        if (s_state == BHV_DOCKED || s_state == BHV_RETURN_HOME) {
            task_dock_request_leave();
            enter(BHV_IDLE);
        }
        break;
    default: break;
    }
}

static void do_patrol(void)
{
    int64_t now = esp_timer_get_time();
    if ((now - s_last_patrol_cmd_us) / 1000 < PATROL_CMD_INTERVAL_MS) {
        // Re-send last setpoint roughly every nav watchdog window so motors
        // don't brake on us. nav_set_velocity is idempotent at this rate.
        return;
    }
    s_last_patrol_cmd_us = now;

    // Simple random walk. Bias forward (linear > 0 most of the time) and
    // mild angular so the robot wanders rather than spins.
    int linear  = 20 + (rand() % 30);          // 20..50
    int angular = (rand() % 80) - 40;          // -40..40
    if ((rand() % 10) == 0) linear = -20;      // 10% chance to back up

    nav_set_velocity(linear, angular);
}

// -- task -----------------------------------------------------------------

static void behavior_task(void *arg)
{
    EventGroupHandle_t evt = sensors_event_group();
    enter(BHV_IDLE);

    while (1) {
        handle_pending();

        // SOS preemption
        EventBits_t bits = evt ? xEventGroupGetBits(evt) : 0;
        if ((bits & EVT_FALL_DETECTED) && s_state != BHV_SOS_ACTIVE) {
            s_pre_sos_state = s_state;
            nav_emergency_stop();
            task_dock_request_cancel();
            enter(BHV_SOS_ACTIVE);
            // task_sos is already independently subscribed to the same bit
            // and will dispatch SMS+dial. We just freeze motion.
        }

        switch (s_state) {
        case BHV_IDLE:
            // Idle on battery floor — request dock if user hasn't already
            if (battery_is_low()) {
                ESP_LOGW(TAG, "battery low while idle — auto-dock");
                enter(BHV_RETURN_HOME);
                task_dock_request_dock();
            }
            break;

        case BHV_PATROL:
            if (battery_is_low()) {
                ESP_LOGW(TAG, "battery low mid-patrol — returning home");
                enter(BHV_RETURN_HOME);
                task_dock_request_dock();
            } else {
                do_patrol();
            }
            break;

        case BHV_RETURN_HOME: {
            dock_state_t ds = task_dock_state();
            if (ds == DOCK_CHARGING || ds == DOCK_CHARGED) {
                enter(BHV_DOCKED);
            } else if (ds == DOCK_FAULT) {
                ESP_LOGE(TAG, "dock fault — stopping in FAULT");
                enter(BHV_FAULT);
            }
            // task_dock owns motion during this state; we just wait
            break;
        }

        case BHV_DOCKED:
            // Sit and charge. User decides when to leave.
            if (!battery_is_charging()) {
                // Lost contact? task_dock will try to re-contact itself.
                // We just observe.
                if (task_dock_state() == DOCK_IDLE || task_dock_state() == DOCK_FAULT) {
                    ESP_LOGW(TAG, "no longer charging and dock task idle — going IDLE");
                    enter(BHV_IDLE);
                }
            }
            break;

        case BHV_SOS_ACTIVE: {
            uint32_t held_s = (uint32_t)((esp_timer_get_time() - s_state_entered_us) / 1000000LL);
            bool fall_clear = (bits & EVT_FALL_CLEAR) != 0;
            if (held_s >= SOS_RECOVERY_HOLD_S && fall_clear) {
                ESP_LOGI(TAG, "fall cleared, resuming %d", s_pre_sos_state);
                enter(s_pre_sos_state);
            }
            break;
        }

        case BHV_FAULT:
            // wait for /behavior/idle from user
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }
}

void task_behavior_start(void)
{
    xTaskCreate(behavior_task, "behavior", 4096, NULL, 6, NULL);
}
