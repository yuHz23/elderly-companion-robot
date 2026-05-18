/*
 * task_ptz — pan-tilt smooth motion controller.
 *
 * Single FreeRTOS task ticking at 50Hz. At each tick, each axis steps
 * toward its target by speed/50 degrees (so 50 dps default = 1°/tick).
 * The axes are independent; pan and tilt converge at their own pace.
 *
 * Concurrency: targets are written by HTTP handlers from a different
 * task. We use a portMUX_TYPE critical section (cheap on dual-core)
 * rather than a mutex — writes are tiny and never block.
 */

#include "task_ptz.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "servo_pwm.h"

static const char *TAG = "ptz";

#define PTZ_TICK_MS         20         // 50Hz
#define DEFAULT_SPEED_DPS   50

typedef struct {
    uint8_t current;
    uint8_t target;
} axis_t;

static axis_t s_axis[SERVO_COUNT] = {
    [SERVO_PAN]  = { .current = 90, .target = 90 },
    [SERVO_TILT] = { .current = 90, .target = 90 },
};
static uint16_t s_speed_dps = DEFAULT_SPEED_DPS;

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

// -- public setters --------------------------------------------------------

void ptz_set_pan_target(uint8_t angle_deg)
{
    portENTER_CRITICAL(&s_lock);
    s_axis[SERVO_PAN].target = servo_clamp(SERVO_PAN, angle_deg);
    portEXIT_CRITICAL(&s_lock);
}

void ptz_set_tilt_target(uint8_t angle_deg)
{
    portENTER_CRITICAL(&s_lock);
    s_axis[SERVO_TILT].target = servo_clamp(SERVO_TILT, angle_deg);
    portEXIT_CRITICAL(&s_lock);
}

void ptz_center(void)
{
    ptz_set_pan_target(90);
    ptz_set_tilt_target(90);
}

void ptz_park(void)
{
    ptz_set_pan_target(90);
    ptz_set_tilt_target(80);   // slight downward — won't snag on chassis
}

void ptz_set_speed(uint16_t deg_per_sec)
{
    if (deg_per_sec < 10)  deg_per_sec = 10;
    if (deg_per_sec > 200) deg_per_sec = 200;
    portENTER_CRITICAL(&s_lock);
    s_speed_dps = deg_per_sec;
    portEXIT_CRITICAL(&s_lock);
}

uint8_t  ptz_get_pan_current(void)  { return s_axis[SERVO_PAN].current;  }
uint8_t  ptz_get_tilt_current(void) { return s_axis[SERVO_TILT].current; }
uint8_t  ptz_get_pan_target(void)   { return s_axis[SERVO_PAN].target;   }
uint8_t  ptz_get_tilt_target(void)  { return s_axis[SERVO_TILT].target;  }
uint16_t ptz_get_speed(void)        { return s_speed_dps;                }

// -- task ------------------------------------------------------------------

static void step_axis(servo_id_t id, uint8_t step_deg)
{
    int diff = (int)s_axis[id].target - (int)s_axis[id].current;
    if (diff == 0) return;

    uint8_t next;
    if ((diff > 0 ? diff : -diff) <= step_deg) {
        next = s_axis[id].target;
    } else {
        next = (uint8_t)((int)s_axis[id].current + (diff > 0 ? step_deg : -(int)step_deg));
    }
    s_axis[id].current = next;
    servo_set_raw_angle(id, next);
}

static void ptz_task(void *arg)
{
    ESP_LOGI(TAG, "task started — pan=%d tilt=%d speed=%ddps",
             s_axis[SERVO_PAN].current, s_axis[SERVO_TILT].current, s_speed_dps);

    // Move to centre on startup (servo_init() already set duty to 90 but
    // we want s_axis state to match what the servos physically did).
    servo_set_raw_angle(SERVO_PAN,  90);
    servo_set_raw_angle(SERVO_TILT, 90);

    TickType_t last_wake = xTaskGetTickCount();
    while (1) {
        uint16_t speed;
        portENTER_CRITICAL(&s_lock);
        speed = s_speed_dps;
        portEXIT_CRITICAL(&s_lock);

        // step = speed / 50 (ticks/sec); guarantee at least 1° step
        uint8_t step = speed / 50;
        if (step == 0) step = 1;

        portENTER_CRITICAL(&s_lock);
        step_axis(SERVO_PAN,  step);
        step_axis(SERVO_TILT, step);
        portEXIT_CRITICAL(&s_lock);

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(PTZ_TICK_MS));
    }
}

void task_ptz_start(void)
{
    servo_init();
    xTaskCreate(ptz_task, "ptz", 4096, NULL, 4, NULL);
}
