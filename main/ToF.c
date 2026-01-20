#include "tof.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdbool.h>

#include "esp_log.h"
#include "driver/gpio.h"

#include "vl53l1x.h"

#define TAG "TOF"

// How often to read the sensor
#define READ_PERIOD_MS 60

//  Module state, static variables
static tof_config_t s_cfg;

static vl53l1x_i2c_handle_t s_i2c = VL53L1X_I2C_INIT;
static vl53l1x_handle_t s_sensor = VL53L1X_INIT;
static vl53l1x_device_handle_t s_dev = VL53L1X_DEVICE_INIT;

static uint16_t s_latest_mm = 0;
static bool s_has_value = false;
static SemaphoreHandle_t s_mm_mutex = NULL;

//  Small helper for buzzer
static void buzzer_beep(uint32_t on_ms, uint32_t off_ms)
{
    gpio_set_level(s_cfg.buzzer_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(on_ms));
    gpio_set_level(s_cfg.buzzer_gpio, 0);
    vTaskDelay(pdMS_TO_TICKS(off_ms));
}

//  Tasks

static void tof_reader_task(void *pv) // reading distance in mm
{
    (void)pv;

    while (1)
    {
        uint16_t mm = vl53l1x_get_mm(&s_dev);

        // Save the newest distance safely
        xSemaphoreTake(s_mm_mutex, portMAX_DELAY);
        s_latest_mm = mm;
        s_has_value = true;
        xSemaphoreGive(s_mm_mutex);

        vTaskDelay(pdMS_TO_TICKS(READ_PERIOD_MS));
    }
}

static void tof_buzzer_task(void *pv) // controlling buzzer based on distance
{
    (void)pv;

    while (1)
    {
        uint16_t mm = 9999;
        bool ok = false;

        // Read latest distance safely
        xSemaphoreTake(s_mm_mutex, portMAX_DELAY);
        ok = s_has_value;
        mm = s_latest_mm;
        xSemaphoreGive(s_mm_mutex);

        if (!ok)
        {
            gpio_set_level(s_cfg.buzzer_gpio, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // Same buzzer logic as before for close distances
        if (mm > 500)
        {
            gpio_set_level(s_cfg.buzzer_gpio, 0);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        else if (mm > 200)
        {
            buzzer_beep(20, 240);
        }
        else if (mm > 100)
        {
            buzzer_beep(20, 160);
        }
        else
        {
            buzzer_beep(20, 60);
        }

        // Small delay so buzzer task doesn't hog CPU
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

//  Public API 
esp_err_t tof_init_and_start(const tof_config_t *cfg) // initialize and start tasks
{
    if (!cfg)
        return ESP_ERR_INVALID_ARG;
    s_cfg = *cfg;

    // Mutex for distance value
    s_mm_mutex = xSemaphoreCreateMutex();
    if (!s_mm_mutex)
        return ESP_ERR_NO_MEM;

    // Buzzer GPIO
    gpio_set_direction(s_cfg.buzzer_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(s_cfg.buzzer_gpio, 0);

    // I2C wrapper config
    s_i2c.sda_gpio = s_cfg.sda_gpio;
    s_i2c.scl_gpio = s_cfg.scl_gpio;

    // Sensor handle uses the I2C handle
    s_sensor.i2c_handle = &s_i2c;

    // Init sensor
    if (!vl53l1x_init(&s_sensor))
    {
        ESP_LOGE(TAG, "vl53l1x_init failed");
        return ESP_FAIL;
    }

    // Device handle (default address 0x29)
    s_dev.vl53l1x_handle = &s_sensor;
    if (!vl53l1x_add_device(&s_dev))
    {
        ESP_LOGE(TAG, "vl53l1x_add_device failed");
        return ESP_FAIL;
    }

    // Create tasks (simple priorities)
    xTaskCreate(tof_reader_task, "tof_reader", 3072, NULL, 10, NULL);
    xTaskCreate(tof_buzzer_task, "tof_buzzer", 2048, NULL, 5, NULL);

    ESP_LOGI(TAG, "ToF started (reader + buzzer tasks)");
    return ESP_OK;
}

esp_err_t tof_get_latest_mm(uint16_t *out_mm) // get latest distance in mm
{
    if (!out_mm)
        return ESP_ERR_INVALID_ARG;
    if (!s_mm_mutex)
        return ESP_ERR_INVALID_STATE;

    xSemaphoreTake(s_mm_mutex, portMAX_DELAY);
    bool ok = s_has_value;
    uint16_t mm = s_latest_mm;
    xSemaphoreGive(s_mm_mutex);

    if (!ok)
        return ESP_ERR_INVALID_RESPONSE;

    *out_mm = mm;
    return ESP_OK;
}
