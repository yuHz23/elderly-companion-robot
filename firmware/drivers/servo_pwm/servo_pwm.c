/*
 * servo_pwm — LEDC-based 50Hz PWM for hobby servos.
 *
 * Pulse range is the conservative 1.0..2.0 ms window. MG90S can be
 * driven past this (typically 0.5..2.5 ms) for wider travel; if you
 * widen the range, tighten SERVO_*_MIN_DEG / *_MAX_DEG accordingly so
 * the servo gear train never bottoms out.
 */

#include "servo_pwm.h"

#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "servo";

// LEDC configuration — keep clear of camera (timer 0) and motor (timer 2)
#define SERVO_TIMER          LEDC_TIMER_1
#define SERVO_MODE           LEDC_LOW_SPEED_MODE
#define SERVO_RESOLUTION     LEDC_TIMER_14_BIT     // 16384 ticks
#define SERVO_FREQ_HZ        50                    // 20 ms period

#define SERVO_PAN_GPIO       44
#define SERVO_TILT_GPIO      45
#define SERVO_PAN_CH         LEDC_CHANNEL_2
#define SERVO_TILT_CH        LEDC_CHANNEL_3

// 14-bit @ 50Hz → 16384 / 20ms = 819.2 ticks/ms
#define LEDC_TICKS_PER_MS    819
#define PULSE_MIN_US         1000  // 0°
#define PULSE_MAX_US         2000  // 180°

#define NVS_NS               "servo"
#define NVS_KEY_PAN_OFFSET   "pan_off"
#define NVS_KEY_TILT_OFFSET  "tilt_off"

static int8_t s_offset[SERVO_COUNT] = {0, 0};

static uint32_t angle_to_duty(uint8_t angle_deg)
{
    if (angle_deg > 180) angle_deg = 180;
    uint32_t pulse_us = PULSE_MIN_US +
                        ((uint32_t)angle_deg * (PULSE_MAX_US - PULSE_MIN_US)) / 180;
    return (pulse_us * LEDC_TICKS_PER_MS) / 1000;
}

static ledc_channel_t channel_for(servo_id_t id)
{
    return (id == SERVO_PAN) ? SERVO_PAN_CH : SERVO_TILT_CH;
}

uint8_t servo_clamp(servo_id_t id, int angle_deg)
{
    int lo = (id == SERVO_PAN) ? SERVO_PAN_MIN_DEG  : SERVO_TILT_MIN_DEG;
    int hi = (id == SERVO_PAN) ? SERVO_PAN_MAX_DEG  : SERVO_TILT_MAX_DEG;
    if (angle_deg < lo) angle_deg = lo;
    if (angle_deg > hi) angle_deg = hi;
    return (uint8_t)angle_deg;
}

static void load_offsets_from_nvs(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    int8_t v;
    if (nvs_get_i8(h, NVS_KEY_PAN_OFFSET, &v) == ESP_OK)  s_offset[SERVO_PAN]  = v;
    if (nvs_get_i8(h, NVS_KEY_TILT_OFFSET, &v) == ESP_OK) s_offset[SERVO_TILT] = v;
    nvs_close(h);
    ESP_LOGI(TAG, "loaded offsets pan=%d tilt=%d",
             s_offset[SERVO_PAN], s_offset[SERVO_TILT]);
}

void servo_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode      = SERVO_MODE,
        .timer_num       = SERVO_TIMER,
        .duty_resolution = SERVO_RESOLUTION,
        .freq_hz         = SERVO_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t pan = {
        .speed_mode = SERVO_MODE,
        .channel    = SERVO_PAN_CH,
        .timer_sel  = SERVO_TIMER,
        .gpio_num   = SERVO_PAN_GPIO,
        .duty       = angle_to_duty(90),     // park at centre
        .hpoint     = 0,
        .intr_type  = LEDC_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&pan));

    ledc_channel_config_t tilt = pan;
    tilt.channel  = SERVO_TILT_CH;
    tilt.gpio_num = SERVO_TILT_GPIO;
    tilt.duty     = angle_to_duty(90);
    ESP_ERROR_CHECK(ledc_channel_config(&tilt));

    load_offsets_from_nvs();
    ESP_LOGI(TAG, "init OK — pan GPIO%d, tilt GPIO%d, 50Hz, 14-bit",
             SERVO_PAN_GPIO, SERVO_TILT_GPIO);
}

void servo_set_raw_angle(servo_id_t id, uint8_t angle_deg)
{
    uint8_t clamped = servo_clamp(id, (int)angle_deg + s_offset[id]);
    uint32_t duty = angle_to_duty(clamped);
    ledc_set_duty(SERVO_MODE, channel_for(id), duty);
    ledc_update_duty(SERVO_MODE, channel_for(id));
}

void servo_set_offset(servo_id_t id, int8_t offset_deg)
{
    if (offset_deg < -10) offset_deg = -10;
    if (offset_deg >  10) offset_deg =  10;
    s_offset[id] = offset_deg;

    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGW(TAG, "nvs open failed; offset not persisted");
        return;
    }
    const char *key = (id == SERVO_PAN) ? NVS_KEY_PAN_OFFSET : NVS_KEY_TILT_OFFSET;
    nvs_set_i8(h, key, offset_deg);
    nvs_commit(h);
    nvs_close(h);
}

int8_t servo_get_offset(servo_id_t id)
{
    return s_offset[id];
}
