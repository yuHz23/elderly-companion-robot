idf_component_register(
    SRCS "task_schedule.c"
    INCLUDE_DIRS "."
    REQUIRES
        task_behavior
        nvs_flash
        lwip
)
