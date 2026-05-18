idf_component_register(
    SRCS "mpu6050.c"
    INCLUDE_DIRS "."
    REQUIRES
        i2c_bus
        nvs_flash
)
