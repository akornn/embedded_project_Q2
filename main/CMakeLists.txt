idf_component_register(SRCS "buzzer.c"
                       PRIV_REQUIRES spi_flash
                       PRIV_REQUIRES esp_driver_gpio
                       PRIV_REQUIRES esp_timer
                       INCLUDE_DIRS "")
