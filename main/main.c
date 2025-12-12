#include <stdio.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c.h"
#include "esp_err.h"

// I2C CONFIG
#define I2C_MASTER_SCL_IO    22      // SCL pin
#define I2C_MASTER_SDA_IO    21      // SDA pin
#define I2C_MASTER_NUM       I2C_NUM_0
#define I2C_MASTER_FREQ_HZ   100000  // 100 kHz (safe)
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0

// MPU6050 CONFIG
#define MPU6050_ADDR             0x68

#define MPU6050_REG_PWR_MGMT_1   0x6B
#define MPU6050_REG_ACCEL_CONFIG 0x1C
#define MPU6050_REG_ACCEL_XOUT_H 0x3B  

#define MPU6050_ACCEL_SENS_2G    16384.0f
#define MPU6050_GYRO_SENS_250    131.0f

// I2C INIT
static void i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode,
                       I2C_MASTER_RX_BUF_DISABLE,
                       I2C_MASTER_TX_BUF_DISABLE, 0);
}

// SMALL HELPERS
static esp_err_t mpu_write_byte(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = { reg, value };
    return i2c_master_write_to_device(I2C_MASTER_NUM,
                                      MPU6050_ADDR,
                                      buf, 2,
                                      pdMS_TO_TICKS(100));
}

static esp_err_t mpu_read_bytes(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM,
                                        MPU6050_ADDR,
                                        &reg, 1,
                                        data, len,
                                        pdMS_TO_TICKS(100));
}

static esp_err_t mpu_init(void)
{
    esp_err_t err;

    vTaskDelay(pdMS_TO_TICKS(100));   // small delay after power-up

    // Wake up MPU6050
    err = mpu_write_byte(MPU6050_REG_PWR_MGMT_1, 0x00);
    if (err != ESP_OK) return err;

    // Set accelerometer to ±2 g 
    err = mpu_write_byte(MPU6050_REG_ACCEL_CONFIG, 0x00);
    if (err != ESP_OK) return err;


    return ESP_OK;
}

// MAIN APP
void app_main(void)
{
    i2c_master_init();

    if (mpu_init() != ESP_OK) {
        printf("Error initializing MPU6050. Check address/wiring.\n");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    while (1) {
        uint8_t data[14];
        esp_err_t err = mpu_read_bytes(MPU6050_REG_ACCEL_XOUT_H, data, sizeof(data));
        if (err == ESP_OK) {
            // Parse raw values (big-endian)
            int16_t raw_ax = (int16_t)((data[0] << 8) | data[1]);
            int16_t raw_ay = (int16_t)((data[2] << 8) | data[3]);
            int16_t raw_az = (int16_t)((data[4] << 8) | data[5]);
            // int16_t raw_temp = (int16_t)((data[6] << 8) | data[7]); // if you ever want temperature
            int16_t raw_gx = (int16_t)((data[8] << 8) | data[9]);
            int16_t raw_gy = (int16_t)((data[10] << 8) | data[11]);
            int16_t raw_gz = (int16_t)((data[12] << 8) | data[13]);

            // Convert to physical units
            float ax = (float)raw_ax / MPU6050_ACCEL_SENS_2G;   // in g
            float ay = (float)raw_ay / MPU6050_ACCEL_SENS_2G;
            float az = (float)raw_az / MPU6050_ACCEL_SENS_2G;

            float gx = (float)raw_gx / MPU6050_GYRO_SENS_250;   // in deg/s
            float gy = (float)raw_gy / MPU6050_GYRO_SENS_250;
            float gz = (float)raw_gz / MPU6050_GYRO_SENS_250;

            printf("ACC  AX=%.2f g  AY=%.2f g  AZ=%.2f g\n", ax, ay, az);
            printf("GYRO GX=%.2f dps GY=%.2f dps GZ=%.2f dps\n\n", gx, gy, gz);
        } else {
            printf("Read error: %d\n", err);
        }

        vTaskDelay(pdMS_TO_TICKS(200));  // 5 Hz
    }
}