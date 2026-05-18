#pragma once

/*
 * Shared I2C master bus on GPIO21 (SDA) / GPIO22 (SCL), 400 kHz.
 *
 * The MPU6050, SSD1306 OLED (Phase 10), and any future I2C device share
 * this one bus. Each device driver calls i2c_bus_init() during its own
 * init; the wrapper is idempotent so multiple callers are safe.
 *
 * Uses ESP-IDF's legacy i2c.h API. The newer i2c_master.h API is
 * cleaner but adds bus-handle plumbing that doesn't buy us anything
 * with only two devices.
 */

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

void     i2c_bus_init(void);
esp_err_t i2c_bus_write(uint8_t addr, const uint8_t *data, size_t len);
esp_err_t i2c_bus_read(uint8_t addr, uint8_t reg, uint8_t *out, size_t len);
esp_err_t i2c_bus_write_reg(uint8_t addr, uint8_t reg, uint8_t value);
bool      i2c_bus_probe(uint8_t addr);
