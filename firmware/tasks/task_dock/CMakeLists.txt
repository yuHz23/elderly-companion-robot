idf_component_register(
    SRCS "task_dock.c"
    INCLUDE_DIRS "."
    REQUIRES
        battery
        ir_dock
        task_navigation
        task_sensor_fusion
        esp_timer
)
