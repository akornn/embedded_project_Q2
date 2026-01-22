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


    tof_ultra_task_start();
}