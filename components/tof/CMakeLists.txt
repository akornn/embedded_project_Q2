idf_component_register(
    SRCS "tof.c"
    INCLUDE_DIRS "include"
    REQUIRES vl53l1x_uld_esp_wrapper esp_driver_gpio
)