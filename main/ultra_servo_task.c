#include "ultra_servo_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

#include "ultrasonic.h"
#include "servo.h"


// Pin wiring
#define US_TRIG GPIO_NUM_26
#define US_ECHO GPIO_NUM_27

// Distance behavior
// Start vibration at 80cm, ramp faster until 50cm, keep fast below 50cm
#define START_VIB_CM 80
#define FAST_VIB_CM  50

#define MAX_DIST_CM   200
#define MIN_VALID_CM  7    // HC-SR04 blind-zone guard

// Timing
#define MEASURE_DELAY_OK_MS   90
#define MEASURE_DELAY_ERR_MS  250
#define COOLDOWN_AFTER_VIB_MS 120

static const char *TAG = "ULTRA_SERVO";

static ultrasonic_sensor_t us = {
    .trigger_pin = US_TRIG,
    .echo_pin = US_ECHO
};

// “Vibration” with small wiggles + pauses (reduces current spikes)
static void servo_vibrate_soft(int wiggles_per_burst,
                               int wiggle_delay_ms,
                               int pause_ms,
                               int angle_a,
                               int angle_b)
{
    for (int i = 0; i < wiggles_per_burst; i++) {
        servo_set_angle(angle_a);
        vTaskDelay(pdMS_TO_TICKS(wiggle_delay_ms));
        servo_set_angle(angle_b);
        vTaskDelay(pdMS_TO_TICKS(wiggle_delay_ms));
    }
    vTaskDelay(pdMS_TO_TICKS(pause_ms));
}

// Map distance 80..50cm into a speed (ms delay): far = slow, close = fast.
// Returns wiggle_delay_ms.
static int map_delay_from_distance(uint32_t cm)
{
    // clamp to [FAST_VIB_CM, START_VIB_CM]
    if (cm < FAST_VIB_CM) cm = FAST_VIB_CM;
    if (cm > START_VIB_CM) cm = START_VIB_CM;

    // at 80cm -> slow (e.g., 90ms), at 50cm -> fast (e.g., 35ms)
    const int slow_ms = 90;
    const int fast_ms = 35;

    // linear interpolation
    // t = (START - cm) / (START - FAST) in [0..1]
    int numerator = (int)(START_VIB_CM - cm);
    int denom = (int)(START_VIB_CM - FAST_VIB_CM);

    int delay = slow_ms - (numerator * (slow_ms - fast_ms)) / denom;
    if (delay < fast_ms) delay = fast_ms;
    if (delay > slow_ms) delay = slow_ms;
    return delay;
}

static void ultra_servo_task(void *pv)
{
    (void)pv;

    ESP_LOGI(TAG, "Init ultrasonic...");
    ESP_ERROR_CHECK(ultrasonic_init(&us));

    ESP_LOGI(TAG, "Init servo...");
    servo_init();
    servo_set_angle(0);
    vTaskDelay(pdMS_TO_TICKS(300));

    // Servo proof-of-life
    ESP_LOGI(TAG, "Servo self-test sweep...");
    servo_set_angle(-50);
    vTaskDelay(pdMS_TO_TICKS(500));
    servo_set_angle(50);
    vTaskDelay(pdMS_TO_TICKS(500));
    servo_set_angle(0);
    vTaskDelay(pdMS_TO_TICKS(300));
    ESP_LOGI(TAG, "Servo test done.");

    static uint32_t last_cm = 999;

    while (1) {
        uint32_t cm = 0;
        esp_err_t err = ultrasonic_measure_cm(&us, MAX_DIST_CM, &cm);

        if (err == ESP_OK && cm > 0 && cm <= MAX_DIST_CM) {
            if (cm < MIN_VALID_CM) cm = MIN_VALID_CM;

            last_cm = cm;
            ESP_LOGI(TAG, "Distance: %lu cm", (unsigned long)last_cm);

            if (last_cm <= START_VIB_CM) {
                // Compute speed based on distance (80..50 ramps to faster)
                int wiggle_delay_ms = map_delay_from_distance(last_cm);

                // Below 50cm, stay at max speed
                if (last_cm <= FAST_VIB_CM) {
                    wiggle_delay_ms = 30;   // fastest
                }

                // Optional: slight angle increase when close
                int angle = (last_cm <= FAST_VIB_CM) ? 30 : 20;

                // Wiggles + pause. Pause helps prevent power spikes/USB issues.
                servo_vibrate_soft(
                    4,                   // wiggles per burst
                    wiggle_delay_ms,     // speed (smaller = faster)
                    120,                 // pause between bursts
                    -angle,
                    angle
                );

                vTaskDelay(pdMS_TO_TICKS(COOLDOWN_AFTER_VIB_MS));
            } else {
                // Farther than 80cm -> no vibration
                servo_set_angle(0);
            }

            vTaskDelay(pdMS_TO_TICKS(MEASURE_DELAY_OK_MS));
        } else {
            // Occasional invalid-state/timeouts can happen; don't panic.
            ESP_LOGW(TAG, "Ultrasonic error: %s (%d)", esp_err_to_name(err), err);

            servo_set_angle(0);
            vTaskDelay(pdMS_TO_TICKS(MEASURE_DELAY_ERR_MS));
        }
    }
}

void ultra_servo_task_start(void)
{
    xTaskCreate(ultra_servo_task, "ultra_servo", 4096, NULL, 5, NULL);
}