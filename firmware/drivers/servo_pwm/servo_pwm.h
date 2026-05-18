#pragma once

/*
 * MG90S / SG90 servo driver — LEDC peripheral on ESP32-S3.
 *
 * Two channels: pan (GPIO44) and tilt (GPIO45), 50Hz PWM, 14-bit duty.
 * Angle 0..180 maps linearly to pulse 1.0..2.0 ms. Per-servo offset
 * (compensating manufacturing tolerance) is loaded from NVS on init.
 *
 * Use servo_smooth_move() from task_ptz; servo_set_raw_angle() jumps
 * immediately and should be avoided except for calibration.
 */

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SERVO_PAN  = 0,
    SERVO_TILT = 1,
    SERVO_COUNT
} servo_id_t;

// Soft limits enforced by the driver
#define SERVO_PAN_MIN_DEG    10
#define SERVO_PAN_MAX_DEG   170
#define SERVO_TILT_MIN_DEG   30
#define SERVO_TILT_MAX_DEG  150

void servo_init(void);

// Write angle directly (clamped to soft limits). Use only for calibration —
// task_ptz applies smooth motion via this same call.
void servo_set_raw_angle(servo_id_t id, uint8_t angle_deg);

// Per-servo trim offset persisted in NVS; range -10..+10 deg
void servo_set_offset(servo_id_t id, int8_t offset_deg);
int8_t servo_get_offset(servo_id_t id);

// Convenience
uint8_t servo_clamp(servo_id_t id, int angle_deg);
