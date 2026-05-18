#pragma once

/*
 * task_ptz — pan-tilt motion controller.
 *
 * Runs at 50Hz, interpolates current → target angle at the configured
 * speed (degrees/second). Sits between the HTTP API (or future autotrack
 * logic) and servo_pwm. All state mutations are serialized through the
 * task itself; callers post commands via the public setters.
 */

#include <stdbool.h>
#include <stdint.h>

void task_ptz_start(void);

// All angle values clamped to soft limits by servo_pwm.
void   ptz_set_pan_target(uint8_t angle_deg);
void   ptz_set_tilt_target(uint8_t angle_deg);
void   ptz_center(void);
void   ptz_park(void);   // pan=90, tilt=80 — slight downward gaze
void   ptz_set_speed(uint16_t deg_per_sec);

uint8_t  ptz_get_pan_current(void);
uint8_t  ptz_get_tilt_current(void);
uint8_t  ptz_get_pan_target(void);
uint8_t  ptz_get_tilt_target(void);
uint16_t ptz_get_speed(void);
