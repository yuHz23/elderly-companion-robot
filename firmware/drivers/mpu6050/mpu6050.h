#pragma once

/*
 * MPU6050 6-axis IMU driver — I2C 0x68.
 *
 * Configuration matches sensor-spec.md:
 *   - ±8g accelerometer, ±1000 dps gyroscope
 *   - DLPF 44Hz (filters motor vibration)
 *   - Sample rate 100Hz (sufficient for fall detection)
 *
 * Calibration offsets are persisted in NVS namespace "mpu6050".
 * Run mpu6050_calibrate_now() with the board resting flat (Z up) to
 * capture and save offsets.
 */

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float ax_g, ay_g, az_g;       // accel in g
    float gx_dps, gy_dps, gz_dps; // gyro in degrees/sec
    float pitch_deg;              // -90..+90
    float roll_deg;               // -180..+180
    float accel_mag_g;            // sqrt(ax² + ay² + az²)
    float tilt_deg;               // angle from upright (0° = level, 90° = on side)
} imu_reading_t;

bool mpu6050_init(void);
bool mpu6050_read(imu_reading_t *out);

// Capture current orientation as the "level resting" reference. Saves
// to NVS. Robot must be physically still and flat while this runs.
bool mpu6050_calibrate_now(void);
