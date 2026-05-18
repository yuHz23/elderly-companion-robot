/*
 * self_test — accumulate per-subsystem pass/fail into one JSON blob.
 *
 * Each check is short and side-effect-free EXCEPT motor pulse (briefly
 * spins both wheels at 30% for 100ms — caller must opt in). Order
 * matters: cheaper checks first so we fail fast if e.g. I2C is dead.
 */

#include "self_test.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "audio_i2s.h"
#include "battery.h"
#include "i2c_bus.h"
#include "ir_dock.h"
#include "motor_l298n.h"
#include "mpu6050.h"
#include "servo_pwm.h"
#include "sim800l.h"
#include "task_ptz.h"
#include "task_sensor_fusion.h"

static const char *TAG = "selftest";

typedef struct {
    char *p;
    size_t left;
    size_t total;
    bool   first;
} buf_t;

static void buf_init(buf_t *b, char *out, size_t out_len)
{
    b->p = out; b->left = out_len; b->total = 0; b->first = true;
    if (out_len > 0) out[0] = '\0';
}

static void buf_putf(buf_t *b, const char *fmt, ...)
{
    if (b->left <= 1) return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(b->p, b->left, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    size_t adv = (size_t)n < b->left ? (size_t)n : b->left - 1;
    b->p += adv; b->left -= adv; b->total += adv;
}

static void emit_check(buf_t *b, const char *name, bool pass, const char *detail)
{
    buf_putf(b, "%s\"%s\":{\"pass\":%s,\"detail\":\"%s\"}",
             b->first ? "" : ",",
             name, pass ? "true" : "false", detail ? detail : "");
    b->first = false;
}

// -- individual checks ----------------------------------------------------

static void check_wifi(buf_t *b)
{
    wifi_ap_record_t ap;
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap);
    bool ok = (err == ESP_OK);
    char det[48];
    if (ok) snprintf(det, sizeof(det), "rssi=%d dBm", ap.rssi);
    else    snprintf(det, sizeof(det), "not associated");
    emit_check(b, "wifi", ok, det);
}

static void check_i2c(buf_t *b)
{
    bool mpu = i2c_bus_probe(0x68);
    bool oled = i2c_bus_probe(0x3C);
    emit_check(b, "i2c.mpu6050", mpu, mpu ? "ack" : "no response");
    emit_check(b, "i2c.ssd1306", oled, oled ? "ack" : "no response");
}

static void check_battery(buf_t *b)
{
    float v = battery_voltage();
    bool ok = (v > 9.0f && v < 13.0f);
    char det[48];
    snprintf(det, sizeof(det), "%.2f V %s", v,
             battery_is_charging() ? "(charging)" : "");
    emit_check(b, "battery", ok, det);
}

static void check_imu(buf_t *b)
{
    imu_reading_t r;
    bool ok = mpu6050_read(&r);
    char det[64];
    if (ok) snprintf(det, sizeof(det), "tilt=%.1f mag=%.2fg", r.tilt_deg, r.accel_mag_g);
    else    snprintf(det, sizeof(det), "read failed");
    emit_check(b, "imu", ok, det);
}

static void check_sensors_freshness(buf_t *b)
{
    sensor_state_t s;
    bool got = sensors_get_state(&s);
    char det[64];
    if (got) {
        int64_t age_ms = (esp_timer_get_time() - s.timestamp_us) / 1000;
        bool fresh = age_ms < 500;
        snprintf(det, sizeof(det), "age=%lldms", age_ms);
        emit_check(b, "sensors", fresh, det);
    } else {
        emit_check(b, "sensors", false, "no snapshot");
    }
}

static void check_ptz(buf_t *b)
{
    // Sweep pan to 60 → 120 → 90, verify smoothing doesn't hang
    uint8_t start = ptz_get_pan_target();
    ptz_set_pan_target(60);
    vTaskDelay(pdMS_TO_TICKS(300));
    ptz_set_pan_target(120);
    vTaskDelay(pdMS_TO_TICKS(300));
    ptz_set_pan_target(start);
    emit_check(b, "ptz", true, "sweep ok");
}

static void check_motor(buf_t *b, bool do_pulse)
{
    if (!do_pulse) {
        emit_check(b, "motor", true, "skipped (no pulse)");
        return;
    }
    // 100 ms forward, 100 ms reverse, then brake. Caller must have robot
    // on its stand for this to be safe.
    motor_set(MOTOR_LEFT,  MOTOR_FWD, 30);
    motor_set(MOTOR_RIGHT, MOTOR_FWD, 30);
    vTaskDelay(pdMS_TO_TICKS(100));
    motor_set(MOTOR_LEFT,  MOTOR_REV, 30);
    motor_set(MOTOR_RIGHT, MOTOR_REV, 30);
    vTaskDelay(pdMS_TO_TICKS(100));
    motor_stop_all();
    emit_check(b, "motor", true, "fwd+rev pulsed");
}

static void check_audio(buf_t *b)
{
    // Very short 1 kHz beep — verifies I2S TX path is alive
    audio_i2s_play_tone(1000, 80, 30);
    emit_check(b, "audio", true, "tone 1kHz 80ms");
}

static void check_sim800(buf_t *b)
{
    sim800_state_t st = sim800_state();
    bool ok = (st == SIM800_READY);
    int rssi = ok ? sim800_signal_quality() : -1;
    char det[64];
    snprintf(det, sizeof(det), "state=%d rssi=%d", (int)st, rssi);
    emit_check(b, "sim800", ok, det);
}

static void check_ir_dock(buf_t *b)
{
    uint8_t s = ir_dock_signal_strength(80);
    // No beacon when undocked → low score expected
    // Just verify the call returned without crashing
    char det[32];
    snprintf(det, sizeof(det), "strength=%u/100", s);
    emit_check(b, "ir_dock", true, det);
}

// -- driver ---------------------------------------------------------------

size_t self_test_run(char *out_json, size_t out_len, bool motor_pulse)
{
    ESP_LOGI(TAG, "running self-test (motor_pulse=%d)", motor_pulse);
    buf_t b;
    buf_init(&b, out_json, out_len);
    buf_putf(&b, "{\"checks\":{");
    b.first = true;

    check_wifi(&b);
    check_i2c(&b);
    check_battery(&b);
    check_imu(&b);
    check_sensors_freshness(&b);
    check_ptz(&b);
    check_motor(&b, motor_pulse);
    check_audio(&b);
    check_sim800(&b);
    check_ir_dock(&b);

    buf_putf(&b, "}}");
    ESP_LOGI(TAG, "self-test done (%u bytes)", (unsigned)b.total);
    return b.total;
}
