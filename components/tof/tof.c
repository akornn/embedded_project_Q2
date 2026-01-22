#include "tof.h"
#include "vl53l1x.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TOF";

/* I2C wrapper handle (shared bus, wrapped for VL53L1X) */
static vl53l1x_i2c_handle_t s_i2c = VL53L1X_I2C_INIT;

/* Sensor + device handles */
static vl53l1x_handle_t s_sensor = VL53L1X_INIT;
static vl53l1x_device_handle_t s_dev = VL53L1X_DEVICE_INIT;

/* Latest distance reading (mm) */
static uint16_t s_latest_mm = 0;

/* ================= Internal reader task ================= */
static void tof_reader_task(void *pv)
{
    (void)pv;

    while (1) {
        s_latest_mm = vl53l1x_get_mm(&s_dev);
        vTaskDelay(pdMS_TO_TICKS(50));  // 20 Hz
    }
}

/* ================= Public API ================= */

esp_err_t tof_init(i2c_master_bus_handle_t shared_bus)
{
    if (shared_bus == NULL) {
        ESP_LOGE(TAG, "I2C bus handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    /* Wrap shared I2C bus for VL53L1X driver */
    s_i2c.bus_handle = shared_bus;
    s_i2c.initialized = true;

    /* Bind wrapper to sensor */
    s_sensor.i2c_handle = &s_i2c;

    if (!vl53l1x_init(&s_sensor)) {
        ESP_LOGE(TAG, "VL53L1X init failed");
        return ESP_FAIL;
    }

    /* Attach device */
    s_dev.vl53l1x_handle = &s_sensor;

    if (!vl53l1x_add_device(&s_dev)) {
        ESP_LOGE(TAG, "Failed to add VL53L1X device");
        return ESP_FAIL;
    }

    /* Start background reader */
    xTaskCreate(
        tof_reader_task,
        "tof_reader",
        2048,
        NULL,
        5,
        NULL
    );

    ESP_LOGI(TAG, "ToF initialized (shared I2C bus)");
    return ESP_OK;
}

uint16_t tof_get_latest_mm(void)
{
    return s_latest_mm;
}
