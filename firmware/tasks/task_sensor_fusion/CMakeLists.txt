idf_component_register(
    SRCS "task_sensor_fusion.c"
    INCLUDE_DIRS "."
    REQUIRES
        mpu6050
        hcsr04
        esp_timer
)
