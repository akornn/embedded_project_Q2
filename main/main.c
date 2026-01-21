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
#include "esp_heap_caps.h"

#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"

#include "mqtt_client.h"
#include "driver/i2c.h"

/* ===================== USER CONFIG ===================== */
#define WIFI_SSID      "AndroidAPDF17"
#define WIFI_PASS      "sigmaboy"
#define MQTT_BROKER_URI "mqtt://51.107.4.193:1883"
#define MQTT_TOPIC      "devices/esp32-001/imu"

/* ===================== I2C CONFIG ===================== */
#define I2C_MASTER_SCL_IO    22
#define I2C_MASTER_SDA_IO    21
#define I2C_MASTER_NUM       I2C_NUM_0
#define I2C_MASTER_FREQ_HZ   100000

/* ===================== MPU6050 CONFIG ===================== */
#define MPU6050_ADDR               0x68
#define MPU6050_REG_PWR_MGMT_1     0x6B
#define MPU6050_REG_ACCEL_CONFIG   0x1C
#define MPU6050_REG_GYRO_CONFIG    0x1B
#define MPU6050_REG_ACCEL_XOUT_H   0x3B

#define MPU6050_ACCEL_SENS_2G      16384.0f
#define MPU6050_GYRO_SENS_2000     16.4f

// Your calibrated offsets
#define AX_OFFSET  -742
#define AY_OFFSET  1180
#define AZ_OFFSET  978
#define GX_OFFSET   74
#define GY_OFFSET    4
#define GZ_OFFSET   -2

// Gyro deadband threshold
#define GYRO_DEADBAND 0.1f

// Filter parameters
#define COMPLEMENTARY_ALPHA 0.96f

// PUBLISH RATE CONTROL
#define PUBLISH_RATE_HZ 10
#define PUBLISH_PERIOD_MS (1000 / PUBLISH_RATE_HZ)

// WiFi connection timeout
#define WIFI_CONNECT_TIMEOUT_MS 30000
#define WIFI_RETRY_INTERVAL_MS 5000

/* ===================== GLOBALS ===================== */
static esp_mqtt_client_handle_t mqtt_client = NULL;
static const char *TAG = "ESP32_MPU_MQTT";

// Connection flags (atomic operations recommended)
static volatile bool wifi_connected = false;
static volatile bool mqtt_connected = false;

// Task handles
static TaskHandle_t imu_task_handle = NULL;
static TaskHandle_t mqtt_task_handle = NULL;

// Data queue for sending IMU data to MQTT task
static QueueHandle_t imu_data_queue = NULL;

/* ===================== DATA STRUCTURES ===================== */
typedef struct {
    float roll;
    float pitch;
    float yaw;
} imu_data_t;

/* ===================== WIFI ===================== */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi STA started");
                esp_wifi_connect();
                break;
                
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WiFi connected to AP");
                break;
                
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "WiFi disconnected, retrying in %d ms...", WIFI_RETRY_INTERVAL_MS);
                wifi_connected = false;
                mqtt_connected = false;
                vTaskDelay(pdMS_TO_TICKS(WIFI_RETRY_INTERVAL_MS));
                esp_wifi_connect();
                break;
        }
    } else if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP:
                ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
                ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
                wifi_connected = true;
                break;
        }
    }
}

static void wifi_init_sta(void) {
    ESP_LOGI(TAG, "Initializing WiFi STA...");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize TCP/IP
    ESP_ERROR_CHECK(esp_netif_init());
    
    // Create event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Create default WiFi STA
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    
    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    
    // Configure WiFi STA
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi initialization finished");
}

/* ===================== MQTT ===================== */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    
    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            mqtt_connected = true;
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT disconnected");
            mqtt_connected = false;
            break;
            
        case MQTT_EVENT_PUBLISHED:
            // ESP_LOGD(TAG, "Message published");
            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error: event_id=%d", event->error_handle->error_type);
            mqtt_connected = false;
            break;
            
        default:
            break;
    }
}

static void mqtt_app_start(void) {
    ESP_LOGI(TAG, "Starting MQTT client...");
    
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .session.keepalive = 30,
        .network.disable_auto_reconnect = false,
        .task.stack_size = 4096,
        .task.priority = 5,
    };
    
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!mqtt_client) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return;
    }
    
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}

/* ===================== I2C / MPU6050 ===================== */
static void i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, I2C_MODE_MASTER, 0, 0, 0));
}

static esp_err_t mpu_write(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = { reg, value };
    esp_err_t ret = i2c_master_write_to_device(I2C_MASTER_NUM, MPU6050_ADDR, buf, 2, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MPU write failed: 0x%02X = 0x%02X, error: 0x%X", reg, value, ret);
    }
    return ret;
}

static esp_err_t mpu_read_bytes(uint8_t reg, uint8_t *data, size_t len) {
    esp_err_t ret = i2c_master_write_read_device(I2C_MASTER_NUM, MPU6050_ADDR, &reg, 1, data, len, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MPU read failed: reg=0x%02X, error=0x%X", reg, ret);
    }
    return ret;
}

static esp_err_t mpu_init(void) {
    ESP_LOGI(TAG, "Initializing MPU6050...");
    
    // Wait for MPU to power up
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Wake up MPU (clear sleep bit)
    esp_err_t ret = mpu_write(MPU6050_REG_PWR_MGMT_1, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to wake MPU6050");
        return ret;
    }
    
    // Configure accelerometer (+/- 2g)
    ret = mpu_write(MPU6050_REG_ACCEL_CONFIG, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure accelerometer");
        return ret;
    }
    
    // Configure gyroscope (+/- 2000°/s)
    ret = mpu_write(MPU6050_REG_GYRO_CONFIG, 0x18);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure gyroscope");
        return ret;
    }
    
    ESP_LOGI(TAG, "MPU6050 initialized successfully");
    return ESP_OK;
}

/* ===================== ORIENTATION FILTER ===================== */
typedef struct {
    float roll;
    float pitch;
    float yaw;
    
    float gyro_roll;
    float gyro_pitch;
    float gyro_yaw;
    
    bool initialized;
    
    // Bias calibration
    float gx_bias;
    float gy_bias;
    float gz_bias;
    int calibration_samples;
    bool calibrated;
} orientation_filter_t;

static void orientation_filter_init(orientation_filter_t *filter) {
    memset(filter, 0, sizeof(orientation_filter_t));
    filter->calibration_samples = 1000; // Calibrate over 10 seconds at 100Hz
}

static void orientation_filter_update(orientation_filter_t *filter,
                                      float gx_dps, float gy_dps, float gz_dps,
                                      float ax_g, float ay_g, float az_g,
                                      float dt) {
    
    // Calibration phase
    if (!filter->calibrated) {
        filter->gx_bias += gx_dps;
        filter->gy_bias += gy_dps;
        filter->gz_bias += gz_dps;
        filter->calibration_samples--;
        
        if (filter->calibration_samples <= 0) {
            filter->gx_bias /= 1000.0f;
            filter->gy_bias /= 1000.0f;
            filter->gz_bias /= 1000.0f;
            filter->calibrated = true;
            ESP_LOGI(TAG, "Gyro bias calibrated: %.3f, %.3f, %.3f °/s",
                    filter->gx_bias, filter->gy_bias, filter->gz_bias);
        }
        return;
    }
    
    // Apply bias correction
    gx_dps -= filter->gx_bias;
    gy_dps -= filter->gy_bias;
    gz_dps -= filter->gz_bias;
    
    // Apply deadband
    #define ABS(x) ((x) > 0 ? (x) : -(x))
    if (ABS(gx_dps) < GYRO_DEADBAND) gx_dps = 0;
    if (ABS(gy_dps) < GYRO_DEADBAND) gy_dps = 0;
    if (ABS(gz_dps) < GYRO_DEADBAND) gz_dps = 0;
    
    // Get angles from accelerometer (for absolute reference)
    float acc_roll = atan2f(ay_g, sqrtf(ax_g * ax_g + az_g * az_g)) * 180.0f / M_PI;
    float acc_pitch = atan2f(-ax_g, sqrtf(ay_g * ay_g + az_g * az_g)) * 180.0f / M_PI;
    
    // Initialize filter on first run
    if (!filter->initialized) {
        filter->gyro_roll = acc_roll;
        filter->gyro_pitch = acc_pitch;
        filter->gyro_yaw = 0;
        filter->initialized = true;
    }
    
    // Integrate gyroscope
    filter->gyro_roll += gx_dps * dt;
    filter->gyro_pitch += gy_dps * dt;
    filter->gyro_yaw += gz_dps * dt;
    
    // Complementary filter fusion
    filter->roll = COMPLEMENTARY_ALPHA * filter->gyro_roll + (1.0f - COMPLEMENTARY_ALPHA) * acc_roll;
    filter->pitch = COMPLEMENTARY_ALPHA * filter->gyro_pitch + (1.0f - COMPLEMENTARY_ALPHA) * acc_pitch;
    filter->yaw = filter->gyro_yaw;
    
    // Update internal gyro state
    filter->gyro_roll = filter->roll;
    filter->gyro_pitch = filter->pitch;
    
    // Normalize angles to -180..180 range
    if (filter->roll > 180.0f) filter->roll -= 360.0f;
    if (filter->roll < -180.0f) filter->roll += 360.0f;
    if (filter->pitch > 180.0f) filter->pitch -= 360.0f;
    if (filter->pitch < -180.0f) filter->pitch += 360.0f;
    if (filter->yaw > 180.0f) filter->yaw -= 360.0f;
    if (filter->yaw < -180.0f) filter->yaw += 360.0f;
}

/* ===================== IMU TASK ===================== */
static void imu_task(void *pvParameters) {
    ESP_LOGI(TAG, "IMU task started");
    
    // Initialize I2C and MPU
    i2c_master_init();
    
    // Retry MPU initialization with backoff
    int retry_count = 0;
    while (mpu_init() != ESP_OK) {
        retry_count++;
        if (retry_count > 5) {
            ESP_LOGE(TAG, "Failed to initialize MPU6050 after %d attempts", retry_count);
            vTaskDelete(NULL);
            return;
        }
        ESP_LOGW(TAG, "Retrying MPU initialization (%d/5)...", retry_count);
        vTaskDelay(pdMS_TO_TICKS(1000 * retry_count)); // Exponential backoff
    }
    
    // Initialize filter
    orientation_filter_t filter;
    orientation_filter_init(&filter);
    
    // Timing
    const TickType_t imu_period = pdMS_TO_TICKS(10); // 100Hz
    TickType_t last_wake_time = xTaskGetTickCount();
    float dt = 0.01f;
    
    // Wait for calibration
    ESP_LOGI(TAG, "Place device flat and stationary for calibration...");
    vTaskDelay(pdMS_TO_TICKS(10000)); // 10 seconds for calibration
    
    ESP_LOGI(TAG, "IMU calibration complete, starting data collection");
    
    while (1) {
        uint8_t data[14];
        
        // Read IMU data
        esp_err_t ret = mpu_read_bytes(MPU6050_REG_ACCEL_XOUT_H, data, 14);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to read MPU data, retrying...");
            vTaskDelayUntil(&last_wake_time, imu_period);
            continue;
        }
        
        // Parse sensor data
        int16_t ax = ((data[0] << 8) | data[1]) - AX_OFFSET;
        int16_t ay = ((data[2] << 8) | data[3]) - AY_OFFSET;
        int16_t az = ((data[4] << 8) | data[5]) - AZ_OFFSET;
        int16_t gx = ((data[8] << 8) | data[9]) - GX_OFFSET;
        int16_t gy = ((data[10] << 8) | data[11]) - GY_OFFSET;
        int16_t gz = ((data[12] << 8) | data[13]) - GZ_OFFSET;
        
        // Convert to proper units
        float ax_g = ax / MPU6050_ACCEL_SENS_2G;
        float ay_g = ay / MPU6050_ACCEL_SENS_2G;
        float az_g = az / MPU6050_ACCEL_SENS_2G;
        float gx_dps = gx / MPU6050_GYRO_SENS_2000;
        float gy_dps = gy / MPU6050_GYRO_SENS_2000;
        float gz_dps = gz / MPU6050_GYRO_SENS_2000;
        
        // Update orientation filter
        orientation_filter_update(&filter, gx_dps, gy_dps, gz_dps,
                                 ax_g, ay_g, az_g, dt);
        
        // Only send data if filter is calibrated
        if (filter.calibrated) {
            // Create data packet
            imu_data_t imu_data = {
                .roll = filter.roll,
                .pitch = filter.pitch,
                .yaw = filter.yaw
            };
            
            // Send to MQTT task (non-blocking)
            if (imu_data_queue != NULL) {
                if (xQueueSend(imu_data_queue, &imu_data, 0) != pdTRUE) {
                    // Queue full, drop oldest
                    imu_data_t dummy;
                    xQueueReceive(imu_data_queue, &dummy, 0);
                    xQueueSend(imu_data_queue, &imu_data, 0);
                }
            }
            
            // Log occasionally
            static int log_counter = 0;
            if (++log_counter >= 100) { // Every second at 100Hz
                log_counter = 0;
                ESP_LOGI(TAG, "IMU: R=%.1f°, P=%.1f°, Y=%.1f°", 
                        filter.roll, filter.pitch, filter.yaw);
            }
        }
        
        // Maintain precise timing
        vTaskDelayUntil(&last_wake_time, imu_period);
    }
}

/* ===================== MQTT TASK ===================== */
static void mqtt_task(void *pvParameters) {
    ESP_LOGI(TAG, "MQTT task started");
    
    // Create data queue
    imu_data_queue = xQueueCreate(10, sizeof(imu_data_t));
    if (imu_data_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create IMU data queue");
        vTaskDelete(NULL);
        return;
    }
    
    // Wait for WiFi before starting MQTT
    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    while (!wifi_connected) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    // Start MQTT
    mqtt_app_start();
    
    // Wait for MQTT connection
    ESP_LOGI(TAG, "Waiting for MQTT connection...");
    int mqtt_timeout = 30; // 30 seconds timeout
    while (!mqtt_connected && mqtt_timeout > 0) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        mqtt_timeout--;
    }
    
    if (!mqtt_connected) {
        ESP_LOGE(TAG, "MQTT connection timeout");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "MQTT ready, starting data publishing");
    
    imu_data_t imu_data;
    TickType_t last_publish_time = 0;
    
    while (1) {
        // Check connections
        if (!wifi_connected || !mqtt_connected) {
            ESP_LOGW(TAG, "Connection lost, waiting for reconnect...");
            while (!wifi_connected || !mqtt_connected) {
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            ESP_LOGI(TAG, "Connection restored");
        }
        
        // Receive data from queue
        if (xQueueReceive(imu_data_queue, &imu_data, pdMS_TO_TICKS(PUBLISH_PERIOD_MS)) == pdTRUE) {
            // Rate limit publishing
            TickType_t now = xTaskGetTickCount();
            if (last_publish_time > 0 && 
                pdTICKS_TO_MS(now - last_publish_time) < PUBLISH_PERIOD_MS) {
                continue; // Skip, too soon
            }
            
            // Create payload
            char payload[64];
            snprintf(payload, sizeof(payload),
                    "{\"roll\":%.2f,\"pitch\":%.2f,\"yaw\":%.2f}",
                    imu_data.roll, imu_data.pitch, imu_data.yaw);
            
            // Publish (with error checking)
            int msg_id = esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC, 
                                                payload, 0, 1, 0);
            if (msg_id < 0) {
                ESP_LOGE(TAG, "MQTT publish failed: %d", msg_id);
                mqtt_connected = false;
            } else {
                last_publish_time = now;
                
                // Log occasionally
                static int publish_count = 0;
                if (++publish_count % 10 == 0) { // Every 10 publishes
                    ESP_LOGI(TAG, "Published: %s", payload);
                }
            }
        }
        
        // Small delay to prevent CPU hogging
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ===================== MAIN ===================== */
void app_main(void) {
    // Print startup info
    ESP_LOGI(TAG, "System starting...");
    ESP_LOGI(TAG, "Free heap: %d", esp_get_free_heap_size());
    
    // Initialize WiFi
    wifi_init_sta();
    
    // Wait a moment for WiFi to start
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Create IMU task (higher priority for precise timing)
    xTaskCreatePinnedToCore(imu_task, "imu_task", 4096, NULL, 5, &imu_task_handle, 0);
    if (imu_task_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create IMU task");
        return;
    }
    
    // Create MQTT task (lower priority)
    xTaskCreatePinnedToCore(mqtt_task, "mqtt_task", 4096, NULL, 3, &mqtt_task_handle, 1);
    if (mqtt_task_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create MQTT task");
        vTaskDelete(imu_task_handle);
        return;
    }
    
    ESP_LOGI(TAG, "System started successfully");
    
    // Monitor tasks
    while (1) {
        // Check if tasks are still running
        if (eTaskGetState(imu_task_handle) == eDeleted ||
            eTaskGetState(mqtt_task_handle) == eDeleted) {
            ESP_LOGE(TAG, "A critical task crashed, restarting...");
            esp_restart();
        }
        
        // Log memory usage occasionally
        static int mem_counter = 0;
        if (++mem_counter >= 60) { // Every minute
            mem_counter = 0;
            ESP_LOGI(TAG, "Heap: Free=%d, Min=%d", 
                    esp_get_free_heap_size(),
                    esp_get_minimum_free_heap_size());
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}