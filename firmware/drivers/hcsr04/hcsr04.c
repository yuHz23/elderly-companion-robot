/*
 * hcsr04 — bit-banged ultrasonic ranging.
 *
 * Why bit-bang instead of RMT?
 *   - RMT works great but adds 200 lines of setup for marginal benefit.
 *   - HC-SR04 echoes are 150 µs..38 ms, well within esp_timer resolution.
 *   - The blocking measurement (max 30 ms) sits inside the dedicated
 *     sensor-fusion task at 20 Hz, so it does not stall anything else.
 *
 * Cross-talk is the caller's problem: only ever call hcsr04_read_cm()
 * for one sensor at a time (see task_sensor_fusion round-robin).
 */

#include "hcsr04.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

static const char *TAG = "hcsr04";

#define ECHO_TIMEOUT_US     30000   // 30 ms ~= 5 m

typedef struct { gpio_num_t trig; gpio_num_t echo; } pair_t;

static const pair_t s_pins[HCSR04_COUNT] = {
    [HCSR04_FRONT] = { 35, 36 },
    [HCSR04_BACK]  = { 37, 38 },
    [HCSR04_LEFT]  = { 39, 40 },
    [HCSR04_RIGHT] = { 42, 43 },
};

void hcsr04_init(void)
{
    for (int i = 0; i < HCSR04_COUNT; ++i) {
        gpio_reset_pin(s_pins[i].trig);
        gpio_set_direction(s_pins[i].trig, GPIO_MODE_OUTPUT);
        gpio_set_level(s_pins[i].trig, 0);

        gpio_reset_pin(s_pins[i].echo);
        gpio_set_direction(s_pins[i].echo, GPIO_MODE_INPUT);
    }
    ESP_LOGI(TAG, "init OK (4 sensors)");
}

uint16_t hcsr04_read_cm(hcsr04_id_t id)
{
    if (id >= HCSR04_COUNT) return HCSR04_OUT_OF_RANGE;
    gpio_num_t trig = s_pins[id].trig;
    gpio_num_t echo = s_pins[id].echo;

    // 10 µs HIGH pulse on TRIG
    gpio_set_level(trig, 0);
    esp_rom_delay_us(2);
    gpio_set_level(trig, 1);
    esp_rom_delay_us(10);
    gpio_set_level(trig, 0);

    // Wait for echo HIGH (start)
    int64_t deadline = esp_timer_get_time() + ECHO_TIMEOUT_US;
    while (gpio_get_level(echo) == 0) {
        if (esp_timer_get_time() > deadline) return HCSR04_OUT_OF_RANGE;
    }
    int64_t t_start = esp_timer_get_time();

    // Wait for echo LOW (end)
    deadline = t_start + ECHO_TIMEOUT_US;
    while (gpio_get_level(echo) == 1) {
        if (esp_timer_get_time() > deadline) return HCSR04_OUT_OF_RANGE;
    }
    int64_t pulse_us = esp_timer_get_time() - t_start;

    // Sound speed → 1 cm per 58 µs (round trip)
    uint32_t cm = (uint32_t)(pulse_us / 58);
    if (cm < 2 || cm > 400) return HCSR04_OUT_OF_RANGE;
    return (uint16_t)cm;
}
