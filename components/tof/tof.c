#include "tof.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdbool.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h" 

#include "vl53l1x.h"

#define TAG "TOF_SHARED"
#define READ_PERIOD_MS 60

static tof_config_t s_cfg;
static vl53l1x_handle_t s_sensor = VL53L1X_INIT;
static vl53l1x_device_handle_t s_dev = VL53L1X_DEVICE_INIT;
static i2c_master_dev_handle_t s_vl53l1x_dev_handle; 

static uint16_t s_latest_mm = 0;
static bool s_has_value = false;
static SemaphoreHandle_t s_mm_mutex = NULL;

static void buzzer_beep(uint32_t on_ms, uint32_t off_ms)
{
    gpio_set_level(s_cfg.buzzer_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(on_ms));
    gpio_set_level(s_cfg.buzzer_gpio, 0);
    vTaskDelay(pdMS_TO_TICKS(off_ms));
}

static void tof_reader_task(void *pv)
{
    while (1)
    {
        // Based on your header, this function retrieves the data directly
        uint16_t mm = vl53l1x_get_mm(&s_dev);

        if (xSemaphoreTake(s_mm_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            s_latest_mm = mm;
            s_has_value = (mm > 0 && mm < 65535);
            xSemaphoreGive(s_mm_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(READ_PERIOD_MS));
    }
}

static void tof_buzzer_task(void *pv)
{
    while (1)
    {
        uint16_t mm = 0;
        bool has_val = false;

        if (xSemaphoreTake(s_mm_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            mm = s_latest_mm;
            has_val = s_has_value;
            xSemaphoreGive(s_mm_mutex);
        }

        if (!has_val || mm > 500 || mm == 0)
        {
            gpio_set_level(s_cfg.buzzer_gpio, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        else if (mm > 200) buzzer_beep(20, 240);
        else if (mm > 100) buzzer_beep(20, 160);
        else              buzzer_beep(20, 60);
    }
}

esp_err_t tof_init_and_start(i2c_master_bus_handle_t bus_handle, const tof_config_t *cfg) 
{
    if (!cfg || !bus_handle)
        return ESP_ERR_INVALID_ARG;
    s_cfg = *cfg;

    s_mm_mutex = xSemaphoreCreateMutex();
    if (!s_mm_mutex)
        return ESP_ERR_NO_MEM;

    gpio_set_direction(s_cfg.buzzer_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(s_cfg.buzzer_gpio, 0);

    // 1. Add VL53L1X to the SHARED bus (I2C_NUM_0)
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x29, 
        .scl_speed_hz = 100000,
    };
    
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &s_vl53l1x_dev_handle));

    // 2. Link the handle to the sensor structure
    s_sensor.i2c_handle = (void *)s_vl53l1x_dev_handle; 

    // 3. Initialize using the functions provided in your vl53l1x.h
    if (!vl53l1x_init(&s_sensor))
    {
        ESP_LOGE(TAG, "VL53L1X init failed");
        return ESP_FAIL;
    }

    s_dev.vl53l1x_handle = &s_sensor;
    if (!vl53l1x_add_device(&s_dev))
    {
        ESP_LOGE(TAG, "vl53l1x_add_device failed");
        return ESP_FAIL;
    }

    // NOTE: 'vl53l1x_start_ranging' is removed because it is not in your header

    xTaskCreate(tof_reader_task, "tof_reader", 3072, NULL, 10, NULL);
    xTaskCreate(tof_buzzer_task, "tof_buzzer", 2048, NULL, 5, NULL);

    ESP_LOGI(TAG, "ToF tasks started on shared bus");
    return ESP_OK;
}