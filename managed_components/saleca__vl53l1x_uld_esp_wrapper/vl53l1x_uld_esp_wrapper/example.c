#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "vl53l1x.h"
#include "esp_log.h"

static const char *TAG = "VL53L1X_EXAMPLE";
static const size_t FPS = 1000 / 3;

static const gpio_num_t SCL_GPIO = GPIO_NUM_5;
static const gpio_num_t SDA_GPIO = GPIO_NUM_6;

vl53l1x_handle_t vl53l1x_handle = VL53L1X_INIT;
vl53l1x_device_handle_t vl53l1x_device = VL53L1X_DEVICE_INIT;

void init_component()
{
    vl53l1x_i2c_handle_t vl53l1x_i2c_handle = VL53L1X_I2C_INIT;
    vl53l1x_i2c_handle.scl_gpio = SCL_GPIO;
    vl53l1x_i2c_handle.sda_gpio = SDA_GPIO;

    vl53l1x_handle.i2c_handle = &vl53l1x_i2c_handle;
    if (!vl53l1x_init(&vl53l1x_handle))
    {
        ESP_LOGE(TAG, "vl53l1x initialization failed");
        return;
    }

    vl53l1x_device.vl53l1x_handle = &vl53l1x_handle;
    if (!vl53l1x_add_device(&vl53l1x_device))
    {
        ESP_LOGE(TAG, "failed to add vl53l1x device 0x%02X", vl53l1x_device.i2c_address);
        return;
    }
}

void app_main(void)
{
    init_component();
    while (1)
    {
        if (vl53l1x_handle.initialized)
        {
            ESP_LOGI(TAG, "distance: %d mm", vl53l1x_get_mm(&vl53l1x_device));
        }
        vTaskDelay(pdMS_TO_TICKS(FPS));
    }
}