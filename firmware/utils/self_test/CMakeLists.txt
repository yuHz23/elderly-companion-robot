idf_component_register(
    SRCS "self_test.c"
    INCLUDE_DIRS "."
    REQUIRES
        audio_i2s
        battery
        i2c_bus
        ir_dock
        motor_l298n
        mpu6050
        servo_pwm
        sim800l
        task_ptz
        task_sensor_fusion
        esp_wifi
        esp_timer
)
