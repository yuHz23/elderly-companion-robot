#pragma once

/*
 * task_navigation — differential-drive controller with watchdog timeout.
 *
 * Mixes (linear, angular) commands into left/right motor PWM, applies
 * per-side trim, and brakes both motors if no command arrives within
 * NAV_WATCHDOG_MS (default 500ms). This is the safety net that keeps
 * the robot from running into a wall when WiFi drops.
 *
 * In a later phase, the obstacle-avoidance reflex (Phase 6 ultrasonic)
 * will gate the output here.
 */

#include <stdbool.h>
#include <stdint.h>

#define NAV_WATCHDOG_MS  500

void task_navigation_start(void);

// Range -100..+100. Linear is forward velocity, angular is CCW yaw rate.
// Calling this resets the watchdog timer.
void nav_set_velocity(int8_t linear, int8_t angular);

// Convenience: zero velocity AND brake immediately (no watchdog wait).
void nav_emergency_stop(void);

// Per-side trim in percent, -10..+10. Saved to NVS.
void  nav_set_trim(int8_t left_trim_pct, int8_t right_trim_pct);
int8_t nav_get_left_trim(void);
int8_t nav_get_right_trim(void);

// Read-back for status JSON
int8_t  nav_get_linear(void);
int8_t  nav_get_angular(void);
bool    nav_is_active(void);     // true if watchdog not expired
