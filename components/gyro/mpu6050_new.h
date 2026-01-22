#pragma once
#include "driver/i2c_master.h"
#include "esp_err.h"

typedef struct {
    float roll;
    float pitch;
    float yaw;
} imu_data_t;

/* Initialize MPU6050 on an existing I2C bus */
esp_err_t mpu6050_new_init(i2c_master_bus_handle_t bus);

/* Read raw IMU data and compute orientation */
esp_err_t mpu6050_new_read(imu_data_t *out);
