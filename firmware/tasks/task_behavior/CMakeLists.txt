idf_component_register(
    SRCS "task_behavior.c"
    INCLUDE_DIRS "."
    REQUIRES
        battery
        task_dock
        task_navigation
        task_sensor_fusion
        esp_timer
)
