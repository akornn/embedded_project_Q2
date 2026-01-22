#include "tof_ultra_task.h"

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"

#include "ultrasonic.h"
#include "vl53l1x.h"
#include "servo.h"

static const char *TAG = "DUAL_CONTROL";

/* ===================== VIBRATION CONTROL ===================== */
// Vibrate only at 80cm and closer. Ramp speed until 50cm, then max speed.
#define VIB_ON_CM          80    // start vibrating at 80cm and closer
#define VIB_MAX_CM         50    // at 50cm and closer => max vibration speed

#define VIB_ON_MS          80    // slowest vibration at 80cm (adjust)
#define VIB_MAX_MS         25    // fastest vibration at 50cm (adjust)

#define SERVO_SWING_DEG    20    // +/- swing angle

/* ===================== ULTRASONIC FILTERING ===================== */
#define ULTRA_MIN_VALID_CM     2
#define ULTRA_MAX_VALID_CM     200
#define ULTRA_EMA_ALPHA        0.25f   // lower = smoother
#define ULTRA_MAX_JUMP_CM      30      // reject sudden spikes
#define ULTRA_GOOD_TIMEOUT_MS  500     // if no good samples for this long => stop servo

/* ===================== LOGGING RATE ===================== */
// Always log distance; can be faster when closer (clamped to safe range)
#define LOG_FAR_MS         400   // >80cm
#define LOG_CLOSE_MS       150   // <=50cm

/* ===================== PIN DEFINITIONS ===================== */
#define TOF_SDA_GPIO 21
#define TOF_SCL_GPIO 22
#define US_TRIG_GPIO 26
#define US_ECHO_GPIO 27
#define BUZZER_GPIO  25

/* ===================== THRESHOLDS ===================== */
#define TOF_MAX_BEEP 500  // Buzzer starts at 500mm

/* ===================== HARDWARE HANDLES ===================== */
static vl53l1x_i2c_handle_t s_i2c = VL53L1X_I2C_INIT;
static vl53l1x_handle_t s_sensor = VL53L1X_INIT;
static vl53l1x_device_handle_t s_dev = VL53L1X_DEVICE_INIT;

static ultrasonic_sensor_t s_ultra_dev = {
    .trigger_pin = US_TRIG_GPIO,
    .echo_pin = US_ECHO_GPIO
};

/* ===================== HELPERS ===================== */
static void alert_beep(int on_ms, int off_ms) {
    gpio_set_level(BUZZER_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(on_ms));
    gpio_set_level(BUZZER_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(off_ms));
}

/* ===================== TASK 1: TOF BUZZER ===================== */
static void tof_buzzer_task(void *pv) {
    (void)pv;

    while (1) {
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

/* ===================== TASK 2: ULTRASONIC SERVO ===================== */
static void ultrasonic_servo_task(void *pv) {
    (void)pv;

    servo_init();
    servo_set_angle(0);

    float ema_cm = 0.0f;
    bool ema_init = false;

    TickType_t last_good = 0;
    TickType_t last_log  = 0;

    while (1) {
        uint32_t cm_raw = 0;
        esp_err_t res = ultrasonic_measure_cm(&s_ultra_dev, ULTRA_MAX_VALID_CM, &cm_raw);

        bool valid = (res == ESP_OK) &&
                     (cm_raw >= ULTRA_MIN_VALID_CM) &&
                     (cm_raw <= ULTRA_MAX_VALID_CM);

        if (valid) {
            float x = (float)cm_raw;

            if (!ema_init) {
                ema_cm = x;
                ema_init = true;
                last_good = xTaskGetTickCount();
            } else {
                float diff = x - ema_cm;
                if (diff < 0) diff = -diff;

                if (diff <= ULTRA_MAX_JUMP_CM) {
                    ema_cm = (ULTRA_EMA_ALPHA * x) + ((1.0f - ULTRA_EMA_ALPHA) * ema_cm);
                    last_good = xTaskGetTickCount();
                }
                // else spike -> ignore sample
            }
        }

        // If we haven't had a good sample recently -> stop servo (prevents "stuck vibrating")
        bool recent = ema_init &&
                      ((xTaskGetTickCount() - last_good) < pdMS_TO_TICKS(ULTRA_GOOD_TIMEOUT_MS));

        if (!recent) {
            // still log occasionally so you can see what's happening
            TickType_t now = xTaskGetTickCount();
            if ((now - last_log) >= pdMS_TO_TICKS(LOG_FAR_MS)) {
                ESP_LOGI(TAG, "Ultra res=%s raw=%lu cm | filt:--- (no recent valid)",
                         esp_err_to_name(res), (unsigned long)cm_raw);
                last_log = now;
            }

            servo_set_angle(0);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        /* -------- Always print distance (even when >80cm) -------- */
        // logging interval: far -> slower logs, close -> faster logs
        int log_ms = LOG_FAR_MS;

        if (ema_cm > 0 && ema_cm <= VIB_ON_CM) {
            float d = ema_cm;
            if (d < VIB_MAX_CM) d = VIB_MAX_CM;
            if (d > VIB_ON_CM)  d = VIB_ON_CM;

            // Map [80..50] => [LOG_FAR_MS..LOG_CLOSE_MS]
            float t = (d - VIB_MAX_CM) / (float)(VIB_ON_CM - VIB_MAX_CM);  // 0..1
            log_ms = (int)(LOG_CLOSE_MS + t * (LOG_FAR_MS - LOG_CLOSE_MS));
        }

        TickType_t now = xTaskGetTickCount();
        if ((now - last_log) >= pdMS_TO_TICKS(log_ms)) {
            ESP_LOGI(TAG, "Ultra raw:%lu cm | filt:%.1f cm",
                     (unsigned long)cm_raw, ema_cm);
            last_log = now;
        }

        /* -------- Servo control -------- */
        // Only vibrate at 80cm and closer
        if (ema_cm > 0 && ema_cm <= VIB_ON_CM) {

            // Clamp distance to [VIB_MAX_CM .. VIB_ON_CM]
            float d = ema_cm;
            if (d < VIB_MAX_CM) d = VIB_MAX_CM;
            if (d > VIB_ON_CM)  d = VIB_ON_CM;

            // Map [80..50] => [slow..fast]
            float t = (d - VIB_MAX_CM) / (float)(VIB_ON_CM - VIB_MAX_CM);  // 0..1
            int delay_ms = (int)(VIB_MAX_MS + t * (VIB_ON_MS - VIB_MAX_MS));

            // One vibration cycle (left-right)
            servo_set_angle(SERVO_SWING_DEG);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
            servo_set_angle(-SERVO_SWING_DEG);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));

        } else {
            // >80cm => OFF
            servo_set_angle(0);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

/* ===================== START ===================== */
void tof_ultra_task_start(void) {
    // Buzzer GPIO
    gpio_reset_pin(BUZZER_GPIO);
    gpio_set_direction(BUZZER_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(BUZZER_GPIO, 0);

    // ToF / I2C Setup
    s_i2c.sda_gpio = TOF_SDA_GPIO;
    s_i2c.scl_gpio = TOF_SCL_GPIO;
    s_sensor.i2c_handle = &s_i2c;

    if (vl53l1x_init(&s_sensor)) {
        s_dev.vl53l1x_handle = &s_sensor;
        vl53l1x_add_device(&s_dev);
    } else {
        ESP_LOGE(TAG, "VL53L1X init failed");
    }

    // Ultrasonic Setup
    esp_err_t uerr = ultrasonic_init(&s_ultra_dev);
    if (uerr != ESP_OK) {
        ESP_LOGE(TAG, "Ultrasonic init failed: %s", esp_err_to_name(uerr));
    }

    // Tasks
    xTaskCreate(tof_buzzer_task, "tof_buzzer", 3072, NULL, 5, NULL);
    xTaskCreate(ultrasonic_servo_task, "ultra_servo", 3072, NULL, 5, NULL);
}