#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"



#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"

#include "mqtt_client.h"
#include "driver/i2c_master.h" // UPDATED: Using New Gen Driver

/* ===================== USER CONFIG ===================== */
#define WIFI_SSID      "AndroidAPDF17"
#define WIFI_PASS      "sigmaboy"
#define MQTT_BROKER_URI "mqtt://51.107.4.193:1883"
#define MQTT_TOPIC      "devices/esp32-001/imu"

/* ===================== MPU6050 CONFIG ===================== */
#define MPU6050_ADDR               0x68
#define MPU6050_REG_PWR_MGMT_1     0x6B
#define MPU6050_REG_ACCEL_CONFIG   0x1C
#define MPU6050_REG_GYRO_CONFIG    0x1B
#define MPU6050_REG_ACCEL_XOUT_H   0x3B

#define MPU6050_ACCEL_SENS_2G      16384.0f
#define MPU6050_GYRO_SENS_2000     16.4f

// Calibrated offsets (Adjust these based on your specific sensor)
#define AX_OFFSET  -742
#define AY_OFFSET  1180
#define AZ_OFFSET  978
#define GX_OFFSET   74
#define GY_OFFSET    4
#define GZ_OFFSET   -2

#define COMPLEMENTARY_ALPHA 0.96f
#define WIFI_RETRY_INTERVAL_MS 5000

/* ===================== GLOBALS ===================== */
static esp_mqtt_client_handle_t mqtt_client = NULL;
static const char *TAG = "GYRO_NG";

static volatile bool wifi_connected = false;
static volatile bool mqtt_connected = false;

static QueueHandle_t imu_data_queue = NULL;

// NEW: Handle for this specific MPU6050 device on the I2C bus
static i2c_master_dev_handle_t mpu6050_handle;

typedef struct {
    float roll;
    float pitch;
    float yaw;
} imu_data_t;

/* ===================== WIFI LOGIC ===================== */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            wifi_connected = false;
            mqtt_connected = false;
            vTaskDelay(pdMS_TO_TICKS(WIFI_RETRY_INTERVAL_MS));
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        wifi_connected = true;
    }
}

void wifi_init_sta(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_flash_init();
    }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/* ===================== MQTT LOGIC ===================== */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
    if (event_id == MQTT_EVENT_CONNECTED) {
        mqtt_connected = true;
    } else if (event_id == MQTT_EVENT_DISCONNECTED) {
        mqtt_connected = false;
    }
}

static void mqtt_app_start(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}

/* ===================== NEW I2C PRIMITIVES ===================== */
static esp_err_t mpu_write(uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    // Transmit to the specific device handle
    return i2c_master_transmit(mpu6050_handle, data, 2, -1);
}

static esp_err_t mpu_read_bytes(uint8_t reg, uint8_t *data, size_t len) {
    // Write register address, then read back data
    return i2c_master_transmit_receive(mpu6050_handle, &reg, 1, data, len, -1);
}

static esp_err_t mpu_init(void) {
    vTaskDelay(pdMS_TO_TICKS(100));
    // Wake up the MPU6050
    if (mpu_write(MPU6050_REG_PWR_MGMT_1, 0x00) != ESP_OK) return ESP_FAIL;
    mpu_write(MPU6050_REG_ACCEL_CONFIG, 0x00); // 2G
    mpu_write(MPU6050_REG_GYRO_CONFIG, 0x18);  // 2000 deg/s
    return ESP_OK;
}

/* ===================== ORIENTATION FILTER ===================== */
typedef struct {
    float roll, pitch, yaw;
    float gyro_roll, gyro_pitch, gyro_yaw;
    bool initialized, calibrated;
    float gx_bias, gy_bias, gz_bias;
    int calibration_samples;
} orientation_filter_t;

static void orientation_filter_update(orientation_filter_t *f, float gx, float gy, float gz, float ax, float ay, float az, float dt) {
    if (!f->calibrated) {
        f->gx_bias += gx; f->gy_bias += gy; f->gz_bias += gz;
        if (--f->calibration_samples <= 0) {
            f->gx_bias /= 1000.0f; f->gy_bias /= 1000.0f; f->gz_bias /= 1000.0f;
            f->calibrated = true;
            ESP_LOGI(TAG, "Calibration Complete");
        }
        return;
    }
    gx -= f->gx_bias; gy -= f->gy_bias; gz -= f->gz_bias;
    float acc_roll = atan2f(ay, sqrtf(ax * ax + az * az)) * 180.0f / M_PI;
    float acc_pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / M_PI;
    if (!f->initialized) { f->gyro_roll = acc_roll; f->gyro_pitch = acc_pitch; f->initialized = true; }
    f->gyro_roll += gx * dt; f->gyro_pitch += gy * dt; f->gyro_yaw += gz * dt;
    f->roll = COMPLEMENTARY_ALPHA * f->gyro_roll + (1.0f - COMPLEMENTARY_ALPHA) * acc_roll;
    f->pitch = COMPLEMENTARY_ALPHA * f->gyro_pitch + (1.0f - COMPLEMENTARY_ALPHA) * acc_pitch;
    f->yaw = f->gyro_yaw;
    f->gyro_roll = f->roll; f->gyro_pitch = f->pitch;
}

/* ===================== TASKS ===================== */
void imu_task(void *pvParameters) {
    // 1. Recover the Bus Handle passed from app_main
    i2c_master_bus_handle_t bus_handle = (i2c_master_bus_handle_t)pvParameters;

    // 2. Add MPU6050 to the Shared Bus
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPU6050_ADDR,
        .scl_speed_hz = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &mpu6050_handle));

    // 3. Initialize Sensor Hardware
    while (mpu_init() != ESP_OK) {
        ESP_LOGE(TAG, "MPU6050 Init failed, retrying...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    orientation_filter_t filter = { .calibration_samples = 1000 };
    TickType_t last_wake = xTaskGetTickCount();
    
    while (1) {
        uint8_t data[14];
        if (mpu_read_bytes(MPU6050_REG_ACCEL_XOUT_H, data, 14) == ESP_OK) {
            float ax = (((int16_t)((data[0] << 8) | data[1])) - AX_OFFSET) / MPU6050_ACCEL_SENS_2G;
            float ay = (((int16_t)((data[2] << 8) | data[3])) - AY_OFFSET) / MPU6050_ACCEL_SENS_2G;
            float az = (((int16_t)((data[4] << 8) | data[5])) - AZ_OFFSET) / MPU6050_ACCEL_SENS_2G;
            float gx = (((int16_t)((data[8] << 8) | data[9])) - GX_OFFSET) / MPU6050_GYRO_SENS_2000;
            float gy = (((int16_t)((data[10] << 8) | data[11])) - GY_OFFSET) / MPU6050_GYRO_SENS_2000;
            float gz = (((int16_t)((data[12] << 8) | data[13])) - GZ_OFFSET) / MPU6050_GYRO_SENS_2000;
            
            orientation_filter_update(&filter, gx, gy, gz, ax, ay, az, 0.01f);
            if (filter.calibrated && imu_data_queue) {
                imu_data_t d = { filter.roll, filter.pitch, filter.yaw };
                xQueueSend(imu_data_queue, &d, 0);
            }
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));
    }
}

void mqtt_task(void *pvParameters) {
    imu_data_queue = xQueueCreate(10, sizeof(imu_data_t));
    while (!wifi_connected) vTaskDelay(pdMS_TO_TICKS(1000));
    mqtt_app_start();
    while (!mqtt_connected) vTaskDelay(pdMS_TO_TICKS(1000));
    
    imu_data_t data;
    while (1) {
        if (xQueueReceive(imu_data_queue, &data, portMAX_DELAY)) {
            char payload[128];
            snprintf(payload, sizeof(payload), "{\"roll\":%.2f,\"pitch\":%.2f,\"yaw\":%.2f}", data.roll, data.pitch, data.yaw);
            esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC, payload, 0, 1, 0);
        }
    }
}