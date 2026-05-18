idf_component_register(
    SRCS "task_ptz.c"
    INCLUDE_DIRS "."
    REQUIRES
        servo_pwm
)
