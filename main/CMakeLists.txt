idf_component_register(
    SRCS "tof.c"
    INCLUDE_DIRS "."
    REQUIRES driver freertos vl53l1x
)
