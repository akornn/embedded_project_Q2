#include <stdio.h>
#include "gyro.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "GYRO_COMPONENT";

// I2C CONFIGURATION
// WARNING: If your Buzzer or another sensor uses Pin 22, change SCL to 19!
#define I2C_MASTER_SCL_IO    22      
#define I2C_MASTER_SDA_IO    21      
#define I2C_MASTER_NUM       I2C_NUM_0
#define I2C_MASTER_FREQ_HZ   100000  

// MPU6050 INTERNAL REGISTERS
#define MPU6050_ADDR             0x68
#define MPU6050_REG_PWR_MGMT_1   0x6B
#define MPU6050_REG_ACCEL_CONFIG 0x1C
#define MPU6050_REG_ACCEL_XOUT_H 0x3B  

#define MPU6050_ACCEL_SENS_2G    16384.0f
#define MPU6050_GYRO_SENS_250    131.0f

/**
 * @brief Private helper to write a byte over I2C
 */
static esp_err_t gyro_write_byte(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = { reg, value };
    return i2c_master_write_to_device(I2C_MASTER_NUM, MPU6050_ADDR, buf, 2, pdMS_TO_TICKS(100));
}

/**
 * @brief Private helper to read bytes over I2C
 */
static esp_err_t gyro_read_bytes(uint8_t reg, uint8_t *data, size_t len) {
    return i2c_master_write_read_device(I2C_MASTER_NUM, MPU6050_ADDR, &reg, 1, data, len, pdMS_TO_TICKS(100));
}

/**
 * @brief Public Init function
 */
esp_err_t gyro_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) return err;
    
    err = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    if (err != ESP_OK) return err;

    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Wake up the MPU6050 (It starts in sleep mode)
    err = gyro_write_byte(MPU6050_REG_PWR_MGMT_1, 0x00);
    if (err != ESP_OK) return err;

    // Set accelerometer to ±2g
    err = gyro_write_byte(MPU6050_REG_ACCEL_CONFIG, 0x00);
    if (err != ESP_OK) return err;
    
    ESP_LOGI(TAG, "Gyroscope (MPU6050) initialized successfully");
    return ESP_OK;
}

/**
 * @brief Public Task function
 */
void gyro_task(void *pvParameters) {
    uint8_t data[14];
    ESP_LOGI(TAG, "Gyro Task Started");

    while (1) {
        // Read 14 bytes starting from Accel X High register
        if (gyro_read_bytes(MPU6050_REG_ACCEL_XOUT_H, data, sizeof(data)) == ESP_OK) {
            // Combine high and low bytes
            int16_t raw_ax = (int16_t)((data[0] << 8) | data[1]);
            int16_t raw_ay = (int16_t)((data[2] << 8) | data[3]);
            int16_t raw_az = (int16_t)((data[4] << 8) | data[5]);
            
            // data[6] and [7] are Temperature (skipping for now)

            int16_t raw_gx = (int16_t)((data[8] << 8) | data[9]);
            int16_t raw_gy = (int16_t)((data[10] << 8) | data[11]);
            int16_t raw_gz = (int16_t)((data[12] << 8) | data[13]);

            // Convert to physical units
            float ax = (float)raw_ax / MPU6050_ACCEL_SENS_2G;
            float ay = (float)raw_ay / MPU6050_ACCEL_SENS_2G;
            float az = (float)raw_az / MPU6050_ACCEL_SENS_2G;
            
            float gx = (float)raw_gx / MPU6050_GYRO_SENS_250;
            float gy = (float)raw_gy / MPU6050_GYRO_SENS_250;
            float gz = (float)raw_gz / MPU6050_GYRO_SENS_250;

            printf("ACC [g]: X=%.2f Y=%.2f Z=%.2f | GYR [dps]: X=%.2f Y=%.2f Z=%.2f\n", 
                    ax, ay, az, gx, gy, gz);
        } else {
            ESP_LOGE(TAG, "Failed to read sensor data over I2C");
        }
        
        vTaskDelay(pdMS_TO_TICKS(200)); // 5Hz sampling rate
    }
}