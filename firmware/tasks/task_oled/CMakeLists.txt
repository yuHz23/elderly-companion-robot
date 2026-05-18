idf_component_register(
    SRCS "task_oled.c"
    INCLUDE_DIRS "."
    REQUIRES
        ssd1306
        battery
        sim800l
        task_behavior
        task_dock
        task_sensor_fusion
        esp_netif
        esp_wifi
        esp_timer
)
