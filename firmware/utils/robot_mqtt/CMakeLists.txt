idf_component_register(
    SRCS "robot_mqtt.c"
    INCLUDE_DIRS "."
    REQUIRES
        mqtt
        nvs_flash
        task_behavior
)
