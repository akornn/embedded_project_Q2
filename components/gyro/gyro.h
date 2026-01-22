#ifndef GYRO_H
#define GYRO_H

#include "esp_err.h"

/**
 * @brief Initializes WiFi in Station mode for MQTT communication.
 * This function handles NVS initialization, TCP/IP stack setup, and 
 * registers event handlers for WiFi and IP events.
 */
void wifi_init_sta(void);

/**
 * @brief Task for reading MPU6050 data and calculating orientation.
 * Performs I2C communication, bias calibration, and applies a 
 * complementary filter to output roll, pitch, and yaw.
 * * @param pvParameters Pointer to task parameters (NULL)
 */
void imu_task(void *pvParameters);

/**
 * @brief Task for publishing IMU data to the MQTT broker.
 * Waits for WiFi/MQTT connection and consumes data from the imu_data_queue
 * to publish JSON payloads to the configured topic.
 * * @param pvParameters Pointer to task parameters (NULL)
 */
void mqtt_task(void *pvParameters);

#endif // GYRO_H