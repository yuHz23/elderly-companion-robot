idf_component_register(
    SRCS "task_audio.c"
    INCLUDE_DIRS "."
    REQUIRES
        audio_i2s
        esp_psram
)
