idf_component_register(
    SRCS "task_sos.c"
    INCLUDE_DIRS "."
    REQUIRES
        sim800l
        task_sensor_fusion
        esp_timer
        nvs_flash
)
