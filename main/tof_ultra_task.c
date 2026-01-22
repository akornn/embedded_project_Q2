#include "tof_ultra_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"

// --- ADD THIS LINE TO FIX IMPLICIT DECLARATION ERRORS ---
#include "i2c_handler.h" 

#include "ultrasonic.h"
#include "vl53l1x.h"
#include "servo.h"

static const char *TAG = "TOF_ULTRA";

#define US_TRIG_GPIO 26
#define US_ECHO_GPIO 27
#define BUZZER_GPIO  25 

// Pins for the dedicated ToF bus on I2C_NUM_1
#define TOF_SDA_GPIO 18
#define TOF_SCL_GPIO 19

#define START_VIB_CM 80   
#define TOF_MAX_BEEP 500  

static vl53l1x_handle_t s_sensor = VL53L1X_INIT;
static vl53l1x_device_handle_t s_dev = VL53L1X_DEVICE_INIT;
static ultrasonic_sensor_t s_ultra_dev = {.trigger_pin = US_TRIG_GPIO, .echo_pin = US_ECHO_GPIO};

static void alert_beep(int on_ms, int off_ms) {
    gpio_set_level(BUZZER_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(on_ms));
    gpio_set_level(BUZZER_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(off_ms));
}

static void tof_buzzer_task(void *pv) {
    ESP_LOGI(TAG, "ToF Buzzer Monitoring Task Started");
    while (1) {
        // Retrieve distance from the sensor
        uint16_t mm = vl53l1x_get_mm(&s_dev); 
        
        if (mm > 0 && mm <= TOF_MAX_BEEP) {
            if (mm > 200)      alert_beep(20, 240); 
            else if (mm > 100) alert_beep(20, 160); 
            else               alert_beep(20, 60);  
        } else {
            gpio_set_level(BUZZER_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

static void ultrasonic_servo_task(void *pv) {
    servo_init(); 
    servo_set_angle(0);
    while (1) {
        uint32_t cm = 0;
        if (ultrasonic_measure_cm(&s_ultra_dev, 200, &cm) == ESP_OK && cm > 0 && cm <= START_VIB_CM) { 
            servo_set_angle(20);
            vTaskDelay(pdMS_TO_TICKS(50));
            servo_set_angle(-20);
            vTaskDelay(pdMS_TO_TICKS(50));
        } else {
            servo_set_angle(0); 
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void tof_ultra_task_start(i2c_master_bus_handle_t ignored_handle) {
    gpio_reset_pin(BUZZER_GPIO);
    gpio_set_direction(BUZZER_GPIO, GPIO_MODE_OUTPUT);

    // 1. Setup the library configuration for Port 1
    static vl53l1x_i2c_handle_t i2c_cfg;
    i2c_cfg.i2c_port = I2C_NUM_1;     
    i2c_cfg.sda_gpio = TOF_SDA_GPIO;  // GPIO 18
    i2c_cfg.scl_gpio = TOF_SCL_GPIO;  // GPIO 19
    i2c_cfg.initialized = false;

    s_sensor.i2c_handle = &i2c_cfg;

    // 2. Initialize the master bus and add the device using the handler functions
    if (i2c_master_init(s_sensor.i2c_handle)) { 
        s_dev.i2c_address = 0x29;
        s_dev.scl_speed_hz = 100000;
        s_dev.vl53l1x_handle = &s_sensor;
        
        if (i2c_add_device(&s_dev)) { 
            if (vl53l1x_init(&s_sensor)) { 
                ESP_LOGI(TAG, "ToF initialized on Port 1 (Pins 18/19)");
            }
        }
    }

    ultrasonic_init(&s_ultra_dev);
    xTaskCreate(tof_buzzer_task, "tof_buzzer", 3072, NULL, 5, NULL);
    xTaskCreate(ultrasonic_servo_task, "ultra_servo", 3072, NULL, 5, NULL);
}