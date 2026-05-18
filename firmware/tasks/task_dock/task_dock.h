#pragma once

/*
 * task_dock — auto-docking state machine.
 *
 * Drives the robot from IDLE → SEARCH → APPROACH → CONTACT → CHARGING →
 * CHARGED. Uses task_navigation for motion, ir_dock for beacon
 * detection, and battery for charge-state heuristics.
 *
 * The task only sends velocity commands; the obstacle gate inside
 * task_navigation still protects against running into walls during
 * SEARCH and APPROACH.
 */

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DOCK_IDLE     = 0,
    DOCK_SEARCH   = 1,
    DOCK_APPROACH = 2,
    DOCK_CONTACT  = 3,
    DOCK_CHARGING = 4,
    DOCK_CHARGED  = 5,
    DOCK_FAULT    = 6,
} dock_state_t;

void task_dock_start(void);

// User-initiated commands
bool task_dock_request_dock(void);     // begin docking sequence
void task_dock_request_leave(void);    // drive forward off charger
void task_dock_request_cancel(void);   // abort docking

dock_state_t task_dock_state(void);
const char  *task_dock_state_name(void);
