#pragma once

/*
 * task_sensor_fusion — reads IMU + all 4 ultrasonics at 20Hz and
 * publishes the latest snapshot via xQueueOverwrite. Also raises bits
 * in a global EventGroupHandle_t when fall / obstacle conditions trip.
 *
 * Consumers (task_navigation, task_oled, task_mqtt, etc.) peek the
 * queue when they want the freshest reading — no blocking dequeue.
 */

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

typedef struct {
    // Ultrasonic distances in cm. 0xFFFF = out of range.
    uint16_t dist_front_cm;
    uint16_t dist_back_cm;
    uint16_t dist_left_cm;
    uint16_t dist_right_cm;

    // IMU
    float ax_g, ay_g, az_g;
    float gx_dps, gy_dps, gz_dps;
    float accel_mag_g;
    float pitch_deg;
    float roll_deg;
    float tilt_deg;

    // Derived flags
    bool fall_detected;

    int64_t timestamp_us;
} sensor_state_t;

// Event group bits exported for other tasks
#define EVT_FALL_DETECTED   (1 << 0)
#define EVT_OBSTACLE_FRONT  (1 << 1)
#define EVT_OBSTACLE_BACK   (1 << 2)
#define EVT_OBSTACLE_LEFT   (1 << 3)
#define EVT_OBSTACLE_RIGHT  (1 << 4)
#define EVT_FALL_CLEAR      (1 << 5)   // robot stood back up

#define OBSTACLE_BRAKE_CM   15
#define OBSTACLE_SLOW_CM    40

void task_sensor_fusion_start(void);

// Take a snapshot of the latest sensor state. Returns false if no
// reading is available yet (called before first sensor tick).
bool sensors_get_state(sensor_state_t *out);

// Global event group — created during task_sensor_fusion_start. Other
// tasks call xEventGroupWaitBits / xEventGroupGetBits on this handle.
EventGroupHandle_t sensors_event_group(void);
