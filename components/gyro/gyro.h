#ifndef GYRO_H
#define GYRO_H

#include "esp_err.h"

/**
 * @brief Initializes I2C and the MPU6050 sensor
 * @return ESP_OK on success
 */
esp_err_t gyro_init(void);

/**
 * @brief Task to read and print Accel/Gyro data
 */
void gyro_task(void *pvParameters);

#endif