#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "vl53l1x.h"
#include "tof.h"

static const char *TAG = "TOF_DRIVER";

#define SDA_GPIO 21
#define SCL_GPIO 22
#define BUZZER_PIN 25

// Handles must be static or global to persist outside of init function
static vl53l1x_i2c_handle_t i2c_handle = VL53L1X_I2C_INIT;
static vl53l1x_handle_t sensor = VL53L1X_INIT;
static vl53l1x_device_handle_t sensor_dev = VL53L1X_DEVICE_INIT;

esp_err_t tof_init(void) {
    i2c_handle.sda_gpio = SDA_GPIO;
    i2c_handle.scl_gpio = SCL_GPIO;
    sensor.i2c_handle = &i2c_handle;

    if (!vl53l1x_init(&sensor)) {
        ESP_LOGE(TAG, "VL53L1X Hardware Init Failed!");
        return ESP_FAIL;
    }

    sensor_dev.vl53l1x_handle = &sensor;
    if (!vl53l1x_add_device(&sensor_dev)) {
        ESP_LOGE(TAG, "Device add FAILED!");
        return ESP_FAIL;
    }

    gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);
    ESP_LOGI(TAG, "TOF Initialized Successfully.");
    return ESP_OK;
}

void tof_task(void *pvParameters) {
    while (1) {
        uint16_t mm = vl53l1x_get_mm(&sensor_dev);
        ESP_LOGI(TAG, "Distance = %d mm", mm);

        if (mm > 1500) {
            gpio_set_level(BUZZER_PIN, 0);
        } else if (mm > 700) {
            gpio_set_level(BUZZER_PIN, 1); vTaskDelay(pdMS_TO_TICKS(20));
            gpio_set_level(BUZZER_PIN, 0); vTaskDelay(pdMS_TO_TICKS(120));
        } else if (mm > 350) {
            gpio_set_level(BUZZER_PIN, 1); vTaskDelay(pdMS_TO_TICKS(20));
            gpio_set_level(BUZZER_PIN, 0); vTaskDelay(pdMS_TO_TICKS(80));
        } else {
            gpio_set_level(BUZZER_PIN, 1); vTaskDelay(pdMS_TO_TICKS(20));
            gpio_set_level(BUZZER_PIN, 0); vTaskDelay(pdMS_TO_TICKS(30));
        }
        vTaskDelay(pdMS_TO_TICKS(60));
    }
}