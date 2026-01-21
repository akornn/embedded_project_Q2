#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "buzzer.h"
#include "servo.h"
#include "gyro.h"
#include "ultrasonic.h"
#include "tof.h"
#include "ultra_servo_task.h"
#include "tof_ultra_task.h"

void app_main(void)
{

    // buzzer_init();
    // servo_init();
    // gyro_init();

    // xTaskCreate(buzzer_task, "buzzer_task", 2048, NULL, 5, NULL);
    // xTaskCreate(servo_sweep_task, "servo_task", 2048, NULL, 5, NULL);
    // xTaskCreate(gyro_task,"gyro_task",2048,NULL,5,NULL);

     ultra_servo_task_start();
    tof_ultra_task_start();
}