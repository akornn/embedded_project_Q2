#include "mpu6050_new.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <math.h>
#include <string.h>

#define TAG "MPU6050_NEW"

/* ================= MPU CONFIG ================= */
#define MPU_ADDR 0x68
#define REG_PWR_MGMT_1  0x6B
#define REG_ACCEL_XOUT  0x3B

#define ACCEL_SENS_2G   16384.0f
#define GYRO_SENS_2000  16.4f

#define COMPLEMENTARY_ALPHA 0.96f
#define DT 0.01f   // 100 Hz

/* Offsets (copied from original code) */
#define AX_OFFSET  -742
#define AY_OFFSET  1180
#define AZ_OFFSET  978
#define GX_OFFSET   74
#define GY_OFFSET    4
#define GZ_OFFSET   -2

static i2c_master_dev_handle_t mpu_dev;

/* ================= ORIENTATION FILTER ================= */
typedef struct {
    float roll, pitch, yaw;
    bool initialized;
} filter_t;

static filter_t filter;

/* ================= LOW LEVEL I2C ================= */

static esp_err_t mpu_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(mpu_dev, buf, sizeof(buf), -1);
}

static esp_err_t mpu_read(uint8_t reg, uint8_t *data, size_t len)
{
    esp_err_t ret;
    ret = i2c_master_transmit(mpu_dev, &reg, 1, -1);
    if (ret != ESP_OK) return ret;
    return i2c_master_receive(mpu_dev, data, len, -1);
}

/* ================= PUBLIC API ================= */

esp_err_t mpu6050_new_init(i2c_master_bus_handle_t bus)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPU_ADDR,
        .scl_speed_hz = 100000,
    };

    ESP_ERROR_CHECK(
        i2c_master_bus_add_device(bus, &dev_cfg, &mpu_dev)
    );

    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_ERROR_CHECK(mpu_write(REG_PWR_MGMT_1, 0x00)); // wake up
    memset(&filter, 0, sizeof(filter));

    ESP_LOGI(TAG, "MPU6050 initialized (new API)");
    return ESP_OK;
}

esp_err_t mpu6050_new_read(imu_data_t *out)
{
    uint8_t raw[14];
    ESP_ERROR_CHECK(mpu_read(REG_ACCEL_XOUT, raw, sizeof(raw)));

    int16_t ax = ((raw[0] << 8) | raw[1]) - AX_OFFSET;
    int16_t ay = ((raw[2] << 8) | raw[3]) - AY_OFFSET;
    int16_t az = ((raw[4] << 8) | raw[5]) - AZ_OFFSET;
    int16_t gx = ((raw[8] << 8) | raw[9]) - GX_OFFSET;
    int16_t gy = ((raw[10] << 8) | raw[11]) - GY_OFFSET;
    int16_t gz = ((raw[12] << 8) | raw[13]) - GZ_OFFSET;

    float ax_g = ax / ACCEL_SENS_2G;
    float ay_g = ay / ACCEL_SENS_2G;
    float az_g = az / ACCEL_SENS_2G;
    float gx_dps = gx / GYRO_SENS_2000;
    float gy_dps = gy / GYRO_SENS_2000;
    float gz_dps = gz / GYRO_SENS_2000;

    float acc_roll  = atan2f(ay_g, sqrtf(ax_g*ax_g + az_g*az_g)) * 180.0f / M_PI;
    float acc_pitch = atan2f(-ax_g, sqrtf(ay_g*ay_g + az_g*az_g)) * 180.0f / M_PI;

    if (!filter.initialized) {
        filter.roll = acc_roll;
        filter.pitch = acc_pitch;
        filter.yaw = 0;
        filter.initialized = true;
    }

    filter.roll  = COMPLEMENTARY_ALPHA * (filter.roll  + gx_dps * DT) + (1 - COMPLEMENTARY_ALPHA) * acc_roll;
    filter.pitch = COMPLEMENTARY_ALPHA * (filter.pitch + gy_dps * DT) + (1 - COMPLEMENTARY_ALPHA) * acc_pitch;
    filter.yaw  += gz_dps * DT;

    out->roll  = filter.roll;
    out->pitch = filter.pitch;
    out->yaw   = filter.yaw;

    return ESP_OK;
}
