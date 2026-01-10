#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "buzzer.h"
#include "servo.h"

void app_main(void)
{
    printf("Starting Integrated Project...\n");
    
    buzzer_init();
    servo_init();

    xTaskCreate(buzzer_task, "buzzer_task", 2048, NULL, 5, NULL);
    xTaskCreate(servo_sweep_task, "servo_task", 2048, NULL, 5, NULL);
}