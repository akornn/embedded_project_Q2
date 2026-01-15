#ifndef TOF_H
#define TOF_H

#include "esp_err.h"

/**
 * @brief Initialize the VL53L1X sensor and Buzzer GPIO
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
esp_err_t tof_init(void);

/**
 * @brief Task to measure distance and control buzzer logic
 */
void tof_task(void *pvParameters);

#endif