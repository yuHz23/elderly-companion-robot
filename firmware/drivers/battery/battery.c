/*
 * battery — ADC1 channel 0 (GPIO1) measures VBAT through 100k+33k divider.
 *
 * The "is_full" heuristic intentionally requires VBAT to sit ABOVE 12.4V
 * for several minutes. A bare battery on its own can briefly read >12.4V
 * if you just stopped charging, but only an active charger holds it
 * there. That's exactly the condition we want.
 */

#include "battery.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "battery";

#define ADC_CH         ADC_CHANNEL_0      // GPIO1 on ESP32-S3
#define ADC_UNIT       ADC_UNIT_1
#define ADC_ATTEN      ADC_ATTEN_DB_12    // 0..~3.3V at the pin (was DB_11 pre-IDF5.2)
#define ADC_BITWIDTH   ADC_BITWIDTH_DEFAULT

// Divider: VBAT × 33 / (100 + 33) = VBAT × 0.248
#define DIVIDER_NUM    33
#define DIVIDER_DEN    133

// Battery curve (linear approx, Li-ion 3S)
#define VBAT_FULL_V    12.40f
#define VBAT_LOW_V     10.20f      // ~20% SoC, dock-now threshold
#define VBAT_EMPTY_V   9.00f       // 0%, BMS will cut off soon
#define VBAT_CHRG_V    12.00f      // anything above implies charger active

#define FULL_STEADY_MIN_S  300     // 5 minutes of >12.4V = full

static adc_oneshot_unit_handle_t s_adc = NULL;
static int64_t s_first_full_v_us = 0;     // start of "full" window

bool battery_init(void)
{
    adc_oneshot_unit_init_cfg_t init = {
        .unit_id = ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    if (adc_oneshot_new_unit(&init, &s_adc) != ESP_OK) {
        ESP_LOGE(TAG, "adc unit init failed");
        return false;
    }
    adc_oneshot_chan_cfg_t cfg = {
        .atten    = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH,
    };
    if (adc_oneshot_config_channel(s_adc, ADC_CH, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "adc channel config failed");
        return false;
    }
    ESP_LOGI(TAG, "init OK — ADC1_CH0 (GPIO1) @ 12dB atten");
    return true;
}

float battery_voltage(void)
{
    if (!s_adc) return 0.0f;
    int raw = 0;
    if (adc_oneshot_read(s_adc, ADC_CH, &raw) != ESP_OK) return 0.0f;
    // raw is 12-bit (0..4095) → V at pin (0..3.3V approx); 11dB atten gives full scale ~3.1V
    float v_pin = ((float)raw / 4095.0f) * 3.3f;
    return v_pin * ((float)DIVIDER_DEN / (float)DIVIDER_NUM);
}

uint8_t battery_percent(void)
{
    float v = battery_voltage();
    if (v <= VBAT_EMPTY_V) return 0;
    if (v >= VBAT_FULL_V)  return 100;
    return (uint8_t)(((v - VBAT_EMPTY_V) / (VBAT_FULL_V - VBAT_EMPTY_V)) * 100.0f);
}

bool battery_is_charging(void)
{
    return battery_voltage() > VBAT_CHRG_V;
}

bool battery_is_full(void)
{
    bool above = battery_voltage() >= VBAT_FULL_V;
    int64_t now = esp_timer_get_time();
    if (!above) {
        s_first_full_v_us = 0;
        return false;
    }
    if (s_first_full_v_us == 0) s_first_full_v_us = now;
    return (now - s_first_full_v_us) / 1000000LL >= FULL_STEADY_MIN_S;
}

bool battery_is_low(void)
{
    // Don't flag "low" while charging (voltage is held up by charger)
    return !battery_is_charging() && battery_voltage() < VBAT_LOW_V;
}
