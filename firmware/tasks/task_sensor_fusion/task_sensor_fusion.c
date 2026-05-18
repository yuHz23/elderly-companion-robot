/*
 * task_sensor_fusion — IMU + 4× ultrasonic at 20Hz.
 *
 * Tick layout (50ms total):
 *   t=0     IMU read (~1ms)
 *   t=5     Ultrasonic[0..3] one per tick (round-robin)
 *           → 25Hz per-sensor would over-eat the budget, 5Hz per sensor
 *             is fine for elderly-pace navigation. We rotate every tick.
 *   t=45    Publish snapshot + event bits
 *
 * Fall detection: a state machine wakes up on |a| > 2.5 g, waits 500 ms
 * for the dust to settle, then checks whether the IMU is still tilted
 * > 60° from upright. Only THEN does it raise EVT_FALL_DETECTED. This
 * avoids triggering on every speed bump.
 */

#include "task_sensor_fusion.h"

#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "mpu6050.h"
#include "hcsr04.h"

static const char *TAG = "sensors";

#define TICK_MS               50            // 20Hz
#define FALL_SPIKE_G          2.5f
#define FALL_CONFIRM_TILT_DEG 60.0f
#define FALL_CONFIRM_DELAY_US 500000LL      // 500 ms
#define FALL_COOLDOWN_US     30000000LL     // 30 s
#define FALL_CLEAR_TILT_DEG   30.0f
#define FALL_CLEAR_HOLD_US   10000000LL     // 10 s upright to clear

static QueueHandle_t       s_queue;
static EventGroupHandle_t  s_events;

// Fall detection state machine
typedef enum {
    FS_NORMAL,
    FS_SPIKE,          // awaiting confirmation
    FS_FALLEN,         // raised — wait for cooldown + recovery
} fall_state_t;

static fall_state_t s_fs = FS_NORMAL;
static int64_t s_spike_at_us = 0;
static int64_t s_upright_since_us = 0;
static int64_t s_last_fall_at_us = 0;

// -- public API -----------------------------------------------------------

bool sensors_get_state(sensor_state_t *out)
{
    if (!s_queue) return false;
    return xQueuePeek(s_queue, out, 0) == pdTRUE;
}

EventGroupHandle_t sensors_event_group(void)
{
    return s_events;
}

// -- helpers --------------------------------------------------------------

static void update_obstacle_bits(const sensor_state_t *s)
{
    EventBits_t set = 0, clr = 0;
    if (s->dist_front_cm < OBSTACLE_BRAKE_CM) set |= EVT_OBSTACLE_FRONT; else clr |= EVT_OBSTACLE_FRONT;
    if (s->dist_back_cm  < OBSTACLE_BRAKE_CM) set |= EVT_OBSTACLE_BACK;  else clr |= EVT_OBSTACLE_BACK;
    if (s->dist_left_cm  < OBSTACLE_BRAKE_CM) set |= EVT_OBSTACLE_LEFT;  else clr |= EVT_OBSTACLE_LEFT;
    if (s->dist_right_cm < OBSTACLE_BRAKE_CM) set |= EVT_OBSTACLE_RIGHT; else clr |= EVT_OBSTACLE_RIGHT;
    if (set) xEventGroupSetBits(s_events, set);
    if (clr) xEventGroupClearBits(s_events, clr);
}

static void update_fall_state(sensor_state_t *s)
{
    int64_t now = s->timestamp_us;

    switch (s_fs) {
    case FS_NORMAL:
        if (s->accel_mag_g > FALL_SPIKE_G) {
            s_fs = FS_SPIKE;
            s_spike_at_us = now;
            ESP_LOGW(TAG, "spike %.2fg — fall candidate", s->accel_mag_g);
        }
        s_upright_since_us = now;       // continuously refresh
        break;

    case FS_SPIKE:
        if (now - s_spike_at_us >= FALL_CONFIRM_DELAY_US) {
            if (s->tilt_deg > FALL_CONFIRM_TILT_DEG &&
                now - s_last_fall_at_us > FALL_COOLDOWN_US) {
                s_fs = FS_FALLEN;
                s_last_fall_at_us = now;
                s->fall_detected = true;
                xEventGroupSetBits(s_events, EVT_FALL_DETECTED);
                ESP_LOGE(TAG, "FALL DETECTED — tilt %.1f°", s->tilt_deg);
            } else {
                s_fs = FS_NORMAL;       // false alarm
                ESP_LOGI(TAG, "spike cleared — tilt %.1f° below threshold", s->tilt_deg);
            }
        }
        break;

    case FS_FALLEN:
        s->fall_detected = true;        // still in fallen state
        if (s->tilt_deg < FALL_CLEAR_TILT_DEG) {
            if (now - s_upright_since_us >= FALL_CLEAR_HOLD_US) {
                s_fs = FS_NORMAL;
                xEventGroupClearBits(s_events, EVT_FALL_DETECTED);
                xEventGroupSetBits(s_events, EVT_FALL_CLEAR);
                ESP_LOGI(TAG, "fall cleared — robot upright");
            }
        } else {
            s_upright_since_us = now;   // reset hold timer
        }
        break;
    }
}

// -- task -----------------------------------------------------------------

static void sensor_task(void *arg)
{
    if (!mpu6050_init()) {
        ESP_LOGE(TAG, "IMU init failed — fall detection disabled");
    }
    hcsr04_init();
    ESP_LOGI(TAG, "task started @ %dHz", 1000 / TICK_MS);

    sensor_state_t state = {0};
    state.dist_front_cm = HCSR04_OUT_OF_RANGE;
    state.dist_back_cm  = HCSR04_OUT_OF_RANGE;
    state.dist_left_cm  = HCSR04_OUT_OF_RANGE;
    state.dist_right_cm = HCSR04_OUT_OF_RANGE;

    int ult_idx = 0;
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        // IMU
        imu_reading_t imu;
        if (mpu6050_read(&imu)) {
            state.ax_g = imu.ax_g; state.ay_g = imu.ay_g; state.az_g = imu.az_g;
            state.gx_dps = imu.gx_dps; state.gy_dps = imu.gy_dps; state.gz_dps = imu.gz_dps;
            state.accel_mag_g = imu.accel_mag_g;
            state.pitch_deg = imu.pitch_deg;
            state.roll_deg  = imu.roll_deg;
            state.tilt_deg  = imu.tilt_deg;
        }

        // Ultrasonic — one sensor per tick (round-robin)
        uint16_t cm = hcsr04_read_cm(ult_idx);
        switch (ult_idx) {
            case HCSR04_FRONT: state.dist_front_cm = cm; break;
            case HCSR04_BACK:  state.dist_back_cm  = cm; break;
            case HCSR04_LEFT:  state.dist_left_cm  = cm; break;
            case HCSR04_RIGHT: state.dist_right_cm = cm; break;
        }
        ult_idx = (ult_idx + 1) % HCSR04_COUNT;

        state.timestamp_us = esp_timer_get_time();
        state.fall_detected = (s_fs == FS_FALLEN);
        update_fall_state(&state);
        update_obstacle_bits(&state);

        xQueueOverwrite(s_queue, &state);

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TICK_MS));
    }
}

void task_sensor_fusion_start(void)
{
    s_queue  = xQueueCreate(1, sizeof(sensor_state_t));
    s_events = xEventGroupCreate();
    xTaskCreate(sensor_task, "sensors", 4096, NULL, 5, NULL);
}
