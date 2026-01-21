#include "tof_ultra_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "ultrasonic.h"
#include "vl53l1x.h"
#include "servo.h"

static const char *TAG = "DUAL_CONTROL";

// Pin Definitions
#define TOF_SDA_GPIO 21
#define TOF_SCL_GPIO 22
#define US_TRIG_GPIO 26
#define US_ECHO_GPIO 27
#define BUZZER_GPIO  25 

// Thresholds from your code
#define START_VIB_CM 80   // Servo starts at 80cm
#define TOF_MAX_BEEP 500  // Buzzer starts at 500mm 

// Hardware Handles
static vl53l1x_i2c_handle_t s_i2c = VL53L1X_I2C_INIT;
static vl53l1x_handle_t s_sensor = VL53L1X_INIT;
static vl53l1x_device_handle_t s_dev = VL53L1X_DEVICE_INIT;
static ultrasonic_sensor_t s_ultra_dev = {.trigger_pin = US_TRIG_GPIO, .echo_pin = US_ECHO_GPIO};

// Helper for ToF Buzzer (from your tof.c) 
static void alert_beep(int on_ms, int off_ms) {
    gpio_set_level(BUZZER_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(on_ms));
    gpio_set_level(BUZZER_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(off_ms));
}

// TASK 1: ToF Buzzer Control (Laser Only)
static void tof_buzzer_task(void *pv) {
    while (1) {
        uint16_t mm = vl53l1x_get_mm(&s_dev);

        // Responds only to ToF and only < 500mm 
        if (mm > 0 && mm <= TOF_MAX_BEEP) {
            if (mm > 200)      alert_beep(20, 240); // Slow 
            else if (mm > 100) alert_beep(20, 160); // Mid 
            else               alert_beep(20, 60);  // Fast 
        } else {
            gpio_set_level(BUZZER_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// TASK 2: Ultrasonic Servo Control (Sound Only)
static void ultrasonic_servo_task(void *pv) {
    servo_init(); //
    servo_set_angle(0);

    while (1) {
        uint32_t cm = 0;
        esp_err_t res = ultrasonic_measure_cm(&s_ultra_dev, 200, &cm);

        if (res == ESP_OK && cm > 0 && cm <= START_VIB_CM) { // Activate at 80cm and below
            ESP_LOGI(TAG, "Ultra: %lu cm -> Servo Vibrating", (unsigned long)cm);
            
            // Simple vibration pattern
            servo_set_angle(20);
            vTaskDelay(pdMS_TO_TICKS(50));
            servo_set_angle(-20);
            vTaskDelay(pdMS_TO_TICKS(50));
        } else {
            servo_set_angle(0); // Idle
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void tof_ultra_task_start(void) {
    // GPIO Setup
    gpio_reset_pin(BUZZER_GPIO);
    gpio_set_direction(BUZZER_GPIO, GPIO_MODE_OUTPUT);

    // ToF / I2C Setup
    s_i2c.sda_gpio = TOF_SDA_GPIO;
    s_i2c.scl_gpio = TOF_SCL_GPIO;
    s_sensor.i2c_handle = &s_i2c;
    if (vl53l1x_init(&s_sensor)) {
        s_dev.vl53l1x_handle = &s_sensor;
        vl53l1x_add_device(&s_dev);
    }

    // Ultrasonic Setup
    ultrasonic_init(&s_ultra_dev); //

    // Separate Tasks for independent response 
    xTaskCreate(tof_buzzer_task, "tof_buzzer", 3072, NULL, 5, NULL);
    xTaskCreate(ultrasonic_servo_task, "ultra_servo", 3072, NULL, 5, NULL);
}