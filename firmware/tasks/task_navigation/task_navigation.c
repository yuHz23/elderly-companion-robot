/*
 * task_navigation — differential-drive mixer with watchdog timeout.
 *
 * Architecture: an HTTP request (or autonomous task) calls
 * nav_set_velocity(linear, angular). That writes a snapshot {l, a, ts}
 * into a portMUX-protected struct. Meanwhile a FreeRTOS task at 50Hz:
 *   - reads the latest snapshot,
 *   - bails to motor_stop_all() if (now - ts) > NAV_WATCHDOG_MS,
 *   - otherwise mixes (linear ± angular) into per-side speed and writes
 *     it to motor_l298n.
 *
 * Trim is applied AFTER the mix and BEFORE the dead-zone-aware motor
 * driver; that keeps the user-facing "go straight" gesture honest even
 * if the two motors aren't perfectly matched.
 */

#include "task_navigation.h"

#include <stdlib.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

#include "motor_l298n.h"
#include "task_sensor_fusion.h"

static const char *TAG = "nav";

#define NAV_TICK_MS  20    // 50Hz

#define NVS_NS              "nav"
#define NVS_KEY_LEFT_TRIM   "ltrim"
#define NVS_KEY_RIGHT_TRIM  "rtrim"

typedef struct {
    int8_t   linear;        // -100..+100
    int8_t   angular;       // -100..+100
    int64_t  timestamp_us;
} drive_cmd_t;

static drive_cmd_t s_cmd = { 0, 0, 0 };
static int8_t s_left_trim  = 0;
static int8_t s_right_trim = 0;
static bool   s_active     = false;

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

// -- helpers --------------------------------------------------------------

static int clamp_i(int v, int lo, int hi)
{
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

static void apply_trim_and_drive(int left_signed, int right_signed)
{
    // Trim: scale each side ±10%
    left_signed  = (left_signed  * (100 + s_left_trim))  / 100;
    right_signed = (right_signed * (100 + s_right_trim)) / 100;

    left_signed  = clamp_i(left_signed,  -100, 100);
    right_signed = clamp_i(right_signed, -100, 100);

    motor_dir_t ldir = (left_signed  > 0) ? MOTOR_FWD :
                       (left_signed  < 0) ? MOTOR_REV : MOTOR_BRAKE;
    motor_dir_t rdir = (right_signed > 0) ? MOTOR_FWD :
                       (right_signed < 0) ? MOTOR_REV : MOTOR_BRAKE;

    motor_set(MOTOR_LEFT,  ldir, (uint8_t)abs(left_signed));
    motor_set(MOTOR_RIGHT, rdir, (uint8_t)abs(right_signed));
}

static void load_trim_from_nvs(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    int8_t v;
    if (nvs_get_i8(h, NVS_KEY_LEFT_TRIM,  &v) == ESP_OK) s_left_trim  = v;
    if (nvs_get_i8(h, NVS_KEY_RIGHT_TRIM, &v) == ESP_OK) s_right_trim = v;
    nvs_close(h);
    ESP_LOGI(TAG, "loaded trim: left=%d%% right=%d%%", s_left_trim, s_right_trim);
}

// -- public setters -------------------------------------------------------

void nav_set_velocity(int8_t linear, int8_t angular)
{
    portENTER_CRITICAL(&s_lock);
    s_cmd.linear       = (int8_t)clamp_i(linear,  -100, 100);
    s_cmd.angular      = (int8_t)clamp_i(angular, -100, 100);
    s_cmd.timestamp_us = esp_timer_get_time();
    portEXIT_CRITICAL(&s_lock);
}

void nav_emergency_stop(void)
{
    portENTER_CRITICAL(&s_lock);
    s_cmd.linear = 0;
    s_cmd.angular = 0;
    s_cmd.timestamp_us = 0;     // expire watchdog immediately
    s_active = false;
    portEXIT_CRITICAL(&s_lock);
    motor_stop_all();
}

void nav_set_trim(int8_t left_trim_pct, int8_t right_trim_pct)
{
    s_left_trim  = (int8_t)clamp_i(left_trim_pct,  -10, 10);
    s_right_trim = (int8_t)clamp_i(right_trim_pct, -10, 10);

    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_i8(h, NVS_KEY_LEFT_TRIM,  s_left_trim);
    nvs_set_i8(h, NVS_KEY_RIGHT_TRIM, s_right_trim);
    nvs_commit(h);
    nvs_close(h);
}

int8_t nav_get_left_trim(void)  { return s_left_trim;  }
int8_t nav_get_right_trim(void) { return s_right_trim; }
int8_t nav_get_linear(void)     { return s_cmd.linear; }
int8_t nav_get_angular(void)    { return s_cmd.angular; }
bool   nav_is_active(void)      { return s_active; }

// -- task -----------------------------------------------------------------

static void nav_task(void *arg)
{
    ESP_LOGI(TAG, "task started — watchdog=%dms tick=%dms", NAV_WATCHDOG_MS, NAV_TICK_MS);

    TickType_t last_wake = xTaskGetTickCount();
    while (1) {
        int8_t  linear;
        int8_t  angular;
        int64_t ts;
        portENTER_CRITICAL(&s_lock);
        linear  = s_cmd.linear;
        angular = s_cmd.angular;
        ts      = s_cmd.timestamp_us;
        portEXIT_CRITICAL(&s_lock);

        int64_t now = esp_timer_get_time();
        bool expired = (ts == 0) || ((now - ts) / 1000 > NAV_WATCHDOG_MS);

        if (expired || (linear == 0 && angular == 0)) {
            if (s_active) {
                motor_stop_all();
                s_active = false;
                if (ts != 0) ESP_LOGW(TAG, "watchdog expired — brake");
            }
        } else {
            // Obstacle avoidance gate: throttle linear velocity based on
            // the distance reported by the sensor in the direction we're
            // moving. Rotation is always allowed so the robot can find
            // an escape route on its own.
            sensor_state_t s;
            int gated_linear = linear;
            if (sensors_get_state(&s)) {
                if (linear > 0) {
                    if (s.dist_front_cm < OBSTACLE_BRAKE_CM)      gated_linear = 0;
                    else if (s.dist_front_cm < OBSTACLE_SLOW_CM)
                        gated_linear = linear * s.dist_front_cm / OBSTACLE_SLOW_CM;
                } else if (linear < 0) {
                    if (s.dist_back_cm < OBSTACLE_BRAKE_CM)       gated_linear = 0;
                    else if (s.dist_back_cm < OBSTACLE_SLOW_CM)
                        gated_linear = linear * s.dist_back_cm / OBSTACLE_SLOW_CM;
                }
            }
            int left  = gated_linear + angular;
            int right = gated_linear - angular;
            apply_trim_and_drive(left, right);
            s_active = true;
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(NAV_TICK_MS));
    }
}

void task_navigation_start(void)
{
    motor_init();
    load_trim_from_nvs();
    xTaskCreate(nav_task, "nav", 4096, NULL, 4, NULL);
}
