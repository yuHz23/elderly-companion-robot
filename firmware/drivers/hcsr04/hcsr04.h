#pragma once

/*
 * HC-SR04 ultrasonic ranging — 4 sensors at front / back / left / right.
 *
 * Each call to hcsr04_read_cm() is blocking but bounded (<= 30 ms via
 * timeout). Cross-talk is avoided by NOT having the driver fire two
 * sensors at once; the caller (task_sensor_fusion) round-robins.
 *
 * Echo pins are 5V — board MUST have a 1kΩ + 2kΩ voltage divider on
 * each Echo trace to bring it down to ~3.3 V. See sensor-spec.md §5.1.
 */

#include <stdbool.h>
#include <stdint.h>

#define HCSR04_OUT_OF_RANGE  0xFFFF   // distance unreadable / timeout

typedef enum {
    HCSR04_FRONT = 0,
    HCSR04_BACK  = 1,
    HCSR04_LEFT  = 2,
    HCSR04_RIGHT = 3,
    HCSR04_COUNT
} hcsr04_id_t;

void     hcsr04_init(void);
uint16_t hcsr04_read_cm(hcsr04_id_t id);   // returns cm, or HCSR04_OUT_OF_RANGE
