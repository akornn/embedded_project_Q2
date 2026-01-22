#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include "tof.h"
#include "mpu6050_new.h"

#include "ultra_servo_task.h"
#include "tof_buzzer_task.h"
#include "gyro_task.h"

void app_main(void)
{
    ESP_ERROR_CHECK(i2c_bus_init());

    i2c_master_bus_handle_t bus = i2c_bus_get_handle();
    if (!bus) {
        ESP_LOGE("MAIN", "I2C bus not available");
        return;
    }

    // ESP_ERROR_CHECK(tof_init(bus));
    ESP_ERROR_CHECK(mpu6050_new_init(bus));

    ultra_servo_task_start();
    // tof_buzzer_task_start();
    gyro_task_start();
}
