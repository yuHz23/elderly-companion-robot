#pragma once

/*
 * Battery monitor — VBAT_SENSE ADC1_CH0 (GPIO1) + dock detection.
 *
 * VBAT is read through a 100k+33k voltage divider that scales 12.6V
 * down to ~3.1V (in spec for ADC1 @ 11dB attenuation). Dock contact
 * voltage is read via the same path because when docked the AC adapter
 * holds VBAT at 12.6V regardless of cell state-of-charge.
 *
 * Heuristic "charged":
 *   battery_is_charging() = VBAT > 12.0V (anything that high implies dock
 *                          power, since 3S Li-ion alone caps at 12.6V
 *                          and that's only true at 100% SoC)
 *   battery_is_full()     = VBAT > 12.4V steady for 5 minutes
 */

#include <stdbool.h>
#include <stdint.h>

bool   battery_init(void);
float  battery_voltage(void);    // V at battery terminals
uint8_t battery_percent(void);   // 0..100, linear approximation
bool   battery_is_charging(void);
bool   battery_is_full(void);    // CV-mode complete heuristic
bool   battery_is_low(void);     // < 20 %, robot should dock
