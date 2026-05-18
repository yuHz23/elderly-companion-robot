idf_component_register(
    SRCS "hcsr04.c"
    INCLUDE_DIRS "."
    REQUIRES
        driver
        esp_timer
)
