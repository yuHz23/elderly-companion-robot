/*
 * Elderly Companion Robot — application entry point
 *
 * Phase 3 scope: smoke-test only.
 *   - Init NVS, PSRAM check, WiFi STA, camera, log heap state.
 *   - No motor/servo/sim800 yet — those come in later phases.
 *
 * Once Phase 3 passes (camera stream visible at http://<ip>/stream),
 * additional tasks are spawned from app_main below in subsequent phases.
 */

#include <stdio.h>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_psram.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "smoke_test.h"

static const char *TAG = "main";

static void log_boot_banner(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=========================================");
    ESP_LOGI(TAG, "  Elderly Companion Robot — Phase 3");
    ESP_LOGI(TAG, "  Build: " __DATE__ " " __TIME__);
    ESP_LOGI(TAG, "=========================================");
    ESP_LOGI(TAG, "");
}

static void log_psram_state(void)
{
    if (esp_psram_is_initialized()) {
        size_t size = esp_psram_get_size();
        ESP_LOGI(TAG, "PSRAM initialized: %u MB", (unsigned)(size / (1024 * 1024)));
    } else {
        ESP_LOGE(TAG, "PSRAM NOT initialized — camera will fail. Check sdkconfig.");
    }
}

void app_main(void)
{
    log_boot_banner();
    log_psram_state();

    // NVS — needed by WiFi, also for persisted user config later
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition needs erase, doing it now");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Smoke test: WiFi + camera + heap report
    smoke_test_run();

    // Hand control over to the smoke-test web server. Subsequent phases
    // will replace this with FreeRTOS task spawn (sensor_fusion, vision,
    // voice, motor, mqtt, etc. — see HDSD Phase 10).
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "alive — free heap: %u, min free: %u",
                 (unsigned)esp_get_free_heap_size(),
                 (unsigned)esp_get_minimum_free_heap_size());
    }
}
