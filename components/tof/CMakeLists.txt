idf_component_register(
    SRCS "tof.c"
    INCLUDE_DIRS "include"
    REQUIRES driver freertos vl53l1x
)
