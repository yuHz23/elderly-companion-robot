idf_component_register(
    SRCS
        "main.c"
        "smoke_test.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        esp_wifi
        esp_event
        esp_netif
        esp_http_server
        esp_psram
        esp_timer
        driver
        esp32-camera
        servo_pwm
        task_ptz
)
