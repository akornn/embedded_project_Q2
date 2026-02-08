idf_component_register(SRCS "main.c" "ultra_servo_task.c" "tof_ultra_task.c" "pir_led.c"
                       INCLUDE_DIRS "."
                       REQUIRES oled buzzer servo gyro ultrasonic tof vl53l1x)