idf_component_register(SRCS "main.c" "ultra_servo_task.c" "tof_buzzer_task.c" "gyro_task.c"
                       INCLUDE_DIRS "."
                       REQUIRES i2c_bus buzzer servo gyro ultrasonic tof vl53l1x)