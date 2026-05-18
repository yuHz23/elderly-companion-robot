/*
 * mpu6050 — register-level driver for the GY-521 module.
 *
 * Read flow: a single 14-byte burst from ACCEL_XOUT_H gives accel +
 * temp + gyro atomically. Burst reads avoid tearing between axes which
 * matters when the chip is being shaken (i.e. fall events).
 */

#include "mpu6050.h"

#include <math.h>
#include <string.h>

#include "esp_log.h"
#include "nvs.h"

#include "i2c_bus.h"

static const char *TAG = "mpu6050";

#define MPU_ADDR          0x68

// Registers
#define REG_SMPLRT_DIV    0x19
#define REG_CONFIG        0x1A
#define REG_GYRO_CONFIG   0x1B
#define REG_ACCEL_CONFIG  0x1C
#define REG_ACCEL_XOUT_H  0x3B
#define REG_PWR_MGMT_1    0x6B
#define REG_WHO_AM_I      0x75

// Scale factors at ±8g and ±1000 dps
#define ACCEL_LSB_PER_G   4096.0f
#define GYRO_LSB_PER_DPS  32.8f

#define NVS_NS            "mpu6050"

// Raw offsets (subtracted from raw counts before scaling)
static int16_t s_off_ax = 0, s_off_ay = 0, s_off_az = 0;
static int16_t s_off_gx = 0, s_off_gy = 0, s_off_gz = 0;

// -------------------------------------------------------------------------

static void load_offsets(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    int16_t v;
    if (nvs_get_i16(h, "ax", &v) == ESP_OK) s_off_ax = v;
    if (nvs_get_i16(h, "ay", &v) == ESP_OK) s_off_ay = v;
    if (nvs_get_i16(h, "az", &v) == ESP_OK) s_off_az = v;
    if (nvs_get_i16(h, "gx", &v) == ESP_OK) s_off_gx = v;
    if (nvs_get_i16(h, "gy", &v) == ESP_OK) s_off_gy = v;
    if (nvs_get_i16(h, "gz", &v) == ESP_OK) s_off_gz = v;
    nvs_close(h);
    ESP_LOGI(TAG, "offsets loaded: a=(%d,%d,%d) g=(%d,%d,%d)",
             s_off_ax, s_off_ay, s_off_az, s_off_gx, s_off_gy, s_off_gz);
}

static bool save_offsets(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    nvs_set_i16(h, "ax", s_off_ax);
    nvs_set_i16(h, "ay", s_off_ay);
    nvs_set_i16(h, "az", s_off_az);
    nvs_set_i16(h, "gx", s_off_gx);
    nvs_set_i16(h, "gy", s_off_gy);
    nvs_set_i16(h, "gz", s_off_gz);
    esp_err_t err = nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK;
}

// -------------------------------------------------------------------------

bool mpu6050_init(void)
{
    i2c_bus_init();

    if (!i2c_bus_probe(MPU_ADDR)) {
        ESP_LOGE(TAG, "device not found at 0x%02X — check wiring/pull-ups", MPU_ADDR);
        return false;
    }

    uint8_t who = 0;
    if (i2c_bus_read(MPU_ADDR, REG_WHO_AM_I, &who, 1) != ESP_OK) return false;
    if (who != 0x68 && who != 0x70 && who != 0x71) {
        ESP_LOGW(TAG, "WHO_AM_I=0x%02X (expected 0x68/0x70/0x71)", who);
    }

    // Wake from sleep, use internal 8MHz oscillator
    if (i2c_bus_write_reg(MPU_ADDR, REG_PWR_MGMT_1,  0x00) != ESP_OK) return false;
    if (i2c_bus_write_reg(MPU_ADDR, REG_CONFIG,      0x03) != ESP_OK) return false; // DLPF 44Hz
    if (i2c_bus_write_reg(MPU_ADDR, REG_GYRO_CONFIG, 0x10) != ESP_OK) return false; // ±1000 dps
    if (i2c_bus_write_reg(MPU_ADDR, REG_ACCEL_CONFIG,0x10) != ESP_OK) return false; // ±8g
    if (i2c_bus_write_reg(MPU_ADDR, REG_SMPLRT_DIV,  0x09) != ESP_OK) return false; // 100 Hz

    load_offsets();
    ESP_LOGI(TAG, "init OK (WHO_AM_I=0x%02X)", who);
    return true;
}

// -------------------------------------------------------------------------

static int16_t be16(const uint8_t *b)
{
    return (int16_t)((b[0] << 8) | b[1]);
}

bool mpu6050_read(imu_reading_t *out)
{
    uint8_t buf[14];
    if (i2c_bus_read(MPU_ADDR, REG_ACCEL_XOUT_H, buf, sizeof(buf)) != ESP_OK) {
        return false;
    }

    int16_t ax = be16(buf + 0) - s_off_ax;
    int16_t ay = be16(buf + 2) - s_off_ay;
    int16_t az = be16(buf + 4) - s_off_az;
    int16_t gx = be16(buf + 8) - s_off_gx;
    int16_t gy = be16(buf + 10) - s_off_gy;
    int16_t gz = be16(buf + 12) - s_off_gz;

    out->ax_g = ax / ACCEL_LSB_PER_G;
    out->ay_g = ay / ACCEL_LSB_PER_G;
    out->az_g = az / ACCEL_LSB_PER_G;
    out->gx_dps = gx / GYRO_LSB_PER_DPS;
    out->gy_dps = gy / GYRO_LSB_PER_DPS;
    out->gz_dps = gz / GYRO_LSB_PER_DPS;

    out->accel_mag_g = sqrtf(out->ax_g * out->ax_g +
                             out->ay_g * out->ay_g +
                             out->az_g * out->az_g);

    // Orientation from gravity vector (assumes mostly static — fine for
    // tilt/fall checks; not a real IMU fusion).
    out->pitch_deg = atan2f(out->ax_g,
                            sqrtf(out->ay_g * out->ay_g + out->az_g * out->az_g))
                     * 180.0f / (float)M_PI;
    out->roll_deg  = atan2f(out->ay_g,
                            sqrtf(out->ax_g * out->ax_g + out->az_g * out->az_g))
                     * 180.0f / (float)M_PI;
    out->tilt_deg  = sqrtf(out->pitch_deg * out->pitch_deg +
                           out->roll_deg  * out->roll_deg);
    return true;
}

// -------------------------------------------------------------------------

bool mpu6050_calibrate_now(void)
{
    // Reset offsets temporarily; we'll measure raw values
    s_off_ax = s_off_ay = s_off_az = 0;
    s_off_gx = s_off_gy = s_off_gz = 0;

    const int N = 100;
    int32_t sum_ax=0, sum_ay=0, sum_az=0, sum_gx=0, sum_gy=0, sum_gz=0;

    for (int i = 0; i < N; ++i) {
        uint8_t buf[14];
        if (i2c_bus_read(MPU_ADDR, REG_ACCEL_XOUT_H, buf, sizeof(buf)) != ESP_OK) {
            return false;
        }
        sum_ax += be16(buf + 0);
        sum_ay += be16(buf + 2);
        sum_az += be16(buf + 4);
        sum_gx += be16(buf + 8);
        sum_gy += be16(buf + 10);
        sum_gz += be16(buf + 12);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Accel: expect (0, 0, 1g) when level → Z offset = (avg_z - 1g_lsb)
    s_off_ax = (int16_t)(sum_ax / N);
    s_off_ay = (int16_t)(sum_ay / N);
    s_off_az = (int16_t)((sum_az / N) - (int)ACCEL_LSB_PER_G);
    s_off_gx = (int16_t)(sum_gx / N);
    s_off_gy = (int16_t)(sum_gy / N);
    s_off_gz = (int16_t)(sum_gz / N);

    bool ok = save_offsets();
    ESP_LOGI(TAG, "calibrate done: a=(%d,%d,%d) g=(%d,%d,%d) saved=%d",
             s_off_ax, s_off_ay, s_off_az, s_off_gx, s_off_gy, s_off_gz, ok);
    return ok;
}
