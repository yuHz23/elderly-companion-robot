idf_component_register(
    SRCS "task_navigation.c"
    INCLUDE_DIRS "."
    REQUIRES
        motor_l298n
        task_sensor_fusion
        esp_timer
        nvs_flash
)
