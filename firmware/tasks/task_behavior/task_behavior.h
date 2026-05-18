#pragma once

/*
 * task_behavior — top-level FSM coordinating the rest of the robot.
 *
 *   IDLE         no autonomous action; manual control via web/MQTT
 *   PATROL       random-walk wander with obstacle avoidance
 *   RETURN_HOME  battery low → drive to dock (delegates to task_dock)
 *   DOCKED       sitting on charger, awaiting user command
 *   SOS_ACTIVE   fall detected; freeze motion, defer to task_sos
 *   FAULT        unrecoverable error — manual intervention required
 *
 * SOS_ACTIVE always preempts whatever the FSM was doing; on
 * EVT_FALL_CLEAR (sensor task says robot is upright again) we return
 * to the previous state.
 */

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BHV_IDLE        = 0,
    BHV_PATROL      = 1,
    BHV_RETURN_HOME = 2,
    BHV_DOCKED      = 3,
    BHV_SOS_ACTIVE  = 4,
    BHV_FAULT       = 5,
} behavior_state_t;

void              task_behavior_start(void);
behavior_state_t  task_behavior_state(void);
const char       *task_behavior_state_name(void);

// User-facing transitions (HTTP /behavior/* hits these)
bool task_behavior_request_idle(void);
bool task_behavior_request_patrol(void);
bool task_behavior_request_dock(void);
bool task_behavior_request_leave(void);
