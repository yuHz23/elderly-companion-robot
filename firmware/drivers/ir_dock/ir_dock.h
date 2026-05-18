#pragma once

/*
 * IR dock beacon receiver — TSOP38238 on GPIO5.
 *
 * The TSOP demodulates a 38 kHz carrier and pulls its output LOW for
 * the duration of a valid burst. So "beacon present" = pin reads LOW.
 *
 * For a directional homing signal we'd need two receivers or a phased
 * array. With one receiver we instead let the caller rotate the robot
 * while ir_dock_signal_strength() samples — the angle with the highest
 * sustained-LOW ratio is the beacon direction.
 */

#include <stdbool.h>
#include <stdint.h>

bool    ir_dock_init(void);

// Instantaneous presence: true if pin is LOW right now (carrier detected).
bool    ir_dock_beacon_present(void);

// Sample the pin many times over `window_ms` and return a 0..100 score:
// 100 = continuously LOW the whole window, 0 = continuously HIGH.
// Use during the SEARCH state's rotation to find the heading.
uint8_t ir_dock_signal_strength(uint32_t window_ms);
