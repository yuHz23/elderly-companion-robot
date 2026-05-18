idf_component_register(
    SRCS "sim800l.c"
    INCLUDE_DIRS "."
    REQUIRES
        driver
        esp_driver_uart
)
