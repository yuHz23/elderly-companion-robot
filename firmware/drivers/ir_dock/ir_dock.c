/*
 * ir_dock — TSOP38238 IR beacon receiver wrapper.
 *
 * The TSOP output sits HIGH at rest. When a 38 kHz IR burst from the
 * dock LED reaches the receiver, the chip pulls OUT LOW for the burst
 * duration. We don't decode anything; the dock just blasts continuous
 * 38 kHz and presence == aligned-toward-dock.
 */

#include "ir_dock.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ir_dock";

#define IR_RX_GPIO     5

bool ir_dock_init(void)
{
    gpio_reset_pin(IR_RX_GPIO);
    gpio_set_direction(IR_RX_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(IR_RX_GPIO, GPIO_PULLUP_ONLY);
    ESP_LOGI(TAG, "init OK — TSOP38238 OUT on GPIO%d", IR_RX_GPIO);
    return true;
}

bool ir_dock_beacon_present(void)
{
    return gpio_get_level(IR_RX_GPIO) == 0;
}

uint8_t ir_dock_signal_strength(uint32_t window_ms)
{
    if (window_ms < 10)  window_ms = 10;
    if (window_ms > 2000) window_ms = 2000;

    const uint32_t sample_interval_us = 200;       // 5 kHz sampling — fast enough vs 38 kHz bursts
    const uint32_t total_samples = (window_ms * 1000) / sample_interval_us;

    uint32_t low_count = 0;
    for (uint32_t i = 0; i < total_samples; ++i) {
        if (gpio_get_level(IR_RX_GPIO) == 0) low_count++;
        esp_rom_delay_us(sample_interval_us);
    }
    return (uint8_t)((low_count * 100) / total_samples);
}
