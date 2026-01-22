#include "mpu6050_new.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "GYRO_TASK";

static void gyro_task(void *pv)
{
    (void)pv;

    ESP_LOGI(TAG, "Starting gyro task");

    while (1) {
        imu_data_t data;

        if (mpu6050_new_read(&data) == ESP_OK) {
            ESP_LOGI(TAG,
                "Roll: %.2f Pitch: %.2f Yaw: %.2f",
                data.roll, data.pitch, data.yaw
            );
        } else {
            ESP_LOGW(TAG, "Failed to read MPU data");
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void gyro_task_start(void)
{
    xTaskCreate(gyro_task, "gyro_task", 3072, NULL, 4, NULL);
}
