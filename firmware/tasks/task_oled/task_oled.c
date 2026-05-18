/*
 * task_oled — periodic status display.
 *
 * Each refresh re-paints all 8 pages. The driver pads each line with
 * trailing spaces so leftover characters from prior frames vanish.
 * 2 Hz is plenty: humans don't read faster than that, and the I2C bus
 * is shared with the IMU which polls at 20 Hz.
 */

#include "task_oled.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "battery.h"
#include "ssd1306.h"
#include "task_behavior.h"
#include "task_dock.h"
#include "task_sensor_fusion.h"
#include "sim800l.h"

static const char *TAG = "oled";

static void format_ip(char *out, size_t out_len)
{
    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!sta) { snprintf(out, out_len, "-"); return; }
    esp_netif_ip_info_t ip;
    if (esp_netif_get_ip_info(sta, &ip) != ESP_OK || ip.ip.addr == 0) {
        snprintf(out, out_len, "-");
        return;
    }
    snprintf(out, out_len, IPSTR, IP2STR(&ip.ip));
}

static const char *sim800_name(sim800_state_t s)
{
    switch (s) {
    case SIM800_OFF:         return "off";
    case SIM800_POWERING_ON: return "wake";
    case SIM800_READY:       return "rdy";
    case SIM800_FAULT:       return "ERR";
    }
    return "?";
}

static void oled_task(void *arg)
{
    if (!ssd1306_init()) {
        ESP_LOGE(TAG, "OLED not present — task exiting");
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "task started — 2 Hz refresh");

    char ip[16];
    while (1) {
        format_ip(ip, sizeof(ip));

        sensor_state_t s;
        bool have_sensor = sensors_get_state(&s);

        ssd1306_printf_line(0, "Elderly Bot %us",
                            (unsigned)(esp_timer_get_time() / 1000000LL));
        ssd1306_printf_line(1, "wifi:%s", ip);
        ssd1306_printf_line(2, "batt:%.2fV %u%%%s",
                            battery_voltage(), battery_percent(),
                            battery_is_charging() ? " CHG" :
                            battery_is_low()      ? " LOW" : "");
        ssd1306_printf_line(3, "state:%s", task_behavior_state_name());
        ssd1306_printf_line(4, "dock:%s", task_dock_state_name());
        ssd1306_printf_line(5, "sim:%s", sim800_name(sim800_state()));
        if (have_sensor) {
            uint16_t f = s.dist_front_cm, b = s.dist_back_cm;
            uint16_t l = s.dist_left_cm,  r = s.dist_right_cm;
            ssd1306_printf_line(6, "F%-3u B%-3u L%-3u R%-3u",
                                f > 999 ? 0 : f,
                                b > 999 ? 0 : b,
                                l > 999 ? 0 : l,
                                r > 999 ? 0 : r);
            ssd1306_printf_line(7, "tilt:%.0f%s",
                                s.tilt_deg,
                                s.fall_detected ? " *FALL*" : "");
        } else {
            ssd1306_printf_line(6, "sensors warming");
            ssd1306_printf_line(7, "...");
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void task_oled_start(void)
{
    xTaskCreate(oled_task, "oled", 3072, NULL, 1, NULL);
}
