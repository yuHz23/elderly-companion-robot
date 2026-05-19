idf_component_register(
    SRCS "task_mqtt.c"
    INCLUDE_DIRS "."
    REQUIRES
        battery
        robot_mqtt
        sim800l
        task_behavior
        task_dock
        task_navigation
        task_ptz
        task_sensor_fusion
        esp_timer
)
