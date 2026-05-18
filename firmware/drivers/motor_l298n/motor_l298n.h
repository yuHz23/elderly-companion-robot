#pragma once

/*
 * L298N H-bridge driver — 2 motors (left/right), 20kHz silent PWM.
 *
 * Channel allocation deviates from the HDSD draft: LEDC_CHANNEL_0/1 are
 * reserved (camera XCLK uses ch0), so motors use CHANNEL_4 and CHANNEL_5
 * on TIMER_2. See docs/hardware/drive-spec.md section 3.
 *
 * Direction is set via two GPIOs per motor (truth table in drive-spec.md
 * section 4). Speed is the PWM duty 0..100 % on the EN pin, with the
 * driver internally applying:
 *   - dead-zone compensation (motor doesn't move under ~20 %)
 *   - PWM cap at 70 % (L298N drop is 2.5V, so 70 % ≈ 6.65V — keeps the
 *     6V-rated BO motor in spec)
 */

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MOTOR_LEFT  = 0,
    MOTOR_RIGHT = 1,
    MOTOR_COUNT
} motor_side_t;

typedef enum {
    MOTOR_FWD,
    MOTOR_REV,
    MOTOR_BRAKE,
    MOTOR_COAST,
} motor_dir_t;

void motor_init(void);

// speed_pct: 0..100. Dead-zone compensation applied internally; pass the
// "logical" speed you want and the driver maps it to actual duty.
void motor_set(motor_side_t side, motor_dir_t dir, uint8_t speed_pct);

void motor_stop_all(void);   // brake both motors
void motor_coast_all(void);  // release both motors (free-wheel)
