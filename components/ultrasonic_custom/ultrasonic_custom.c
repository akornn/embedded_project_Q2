// ultrasonic_custom.c - UPDATED VERSION WITH esp_timer

#include "ultrasonic_custom.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define TAG "ULTRASONIC_CUSTOM"
#define TRIG_PULSE_US 10
#define SOUND_SPEED 0.0343
#define MEASUREMENT_INTERVAL_MS 100

static int s_trig_pin = 0;
static int s_echo_pin = 0;
static esp_timer_handle_t s_trigger_timer = NULL;
static uint32_t s_echo_start = 0;
static uint32_t s_echo_end = 0;
static volatile bool s_got_echo = false;
static float s_last_distance = -1.0f;
static uint64_t s_last_measurement_time = 0;

// Timer callback: turns off trigger after 10µs
static void trigger_timer_callback(void *arg) {
    gpio_set_level(s_trig_pin, 0);
}

// Echo pin interrupt handler
static void IRAM_ATTR echo_isr_handler(void *arg) {
    uint32_t now = esp_timer_get_time();
    
    if (gpio_get_level(s_echo_pin)) {
        // Rising edge - echo start
        s_echo_start = now;
    } else {
        // Falling edge - echo end
        if (s_echo_start > 0) {
            s_echo_end = now;
            s_got_echo = true;
        }
    }
}

void ultrasonic_custom_init(int trig_pin, int echo_pin) {
    s_trig_pin = trig_pin;
    s_echo_pin = echo_pin;
    
    ESP_LOGI(TAG, "Initializing ultrasonic on TRIG=%d, ECHO=%d", trig_pin, echo_pin);
    
    // Setup trigger pin
    gpio_config_t trig_cfg = {
        .pin_bit_mask = 1ULL << s_trig_pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = false,
        .pull_up_en = false,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&trig_cfg);
    gpio_set_level(s_trig_pin, 0);
    
    // Setup echo pin
    gpio_config_t echo_cfg = {
        .pin_bit_mask = 1ULL << s_echo_pin,
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = false,
        .pull_up_en = false,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    gpio_config(&echo_cfg);
    
    // Install ISR service if not already installed
    static bool isr_installed = false;
    if (!isr_installed) {
        gpio_install_isr_service(0);
        isr_installed = true;
    }
    
    // Add ISR handler
    gpio_isr_handler_add(s_echo_pin, echo_isr_handler, NULL);
    
    // Create one-shot timer for trigger pulse
    const esp_timer_create_args_t trigger_timer_args = {
        .callback = &trigger_timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "ultrasonic_trigger"
    };
    esp_timer_create(&trigger_timer_args, &s_trigger_timer);
    
    ESP_LOGI(TAG, "Ultrasonic custom init complete");
}

bool ultrasonic_custom_measurement_available(void) {
    return s_got_echo;
}

float ultrasonic_custom_get_distance_cm(void) {
    uint64_t now = esp_timer_get_time() / 1000;  // Convert to ms
    
    // Don't measure too frequently
    if ((now - s_last_measurement_time) < MEASUREMENT_INTERVAL_MS) {
        return s_last_distance;  // Return cached value
    }
    
    // Reset state
    s_got_echo = false;
    s_echo_start = 0;
    s_echo_end = 0;
    
    // Send trigger pulse using esp_timer for precise timing
    gpio_set_level(s_trig_pin, 1);
    esp_timer_start_once(s_trigger_timer, TRIG_PULSE_US);
    
    // Wait for measurement with timeout (100ms)
    int64_t start_time = esp_timer_get_time();
    while (!s_got_echo) {
        if ((esp_timer_get_time() - start_time) > 100000) {  // 100ms timeout
            ESP_LOGW(TAG, "Ultrasonic timeout");
            s_last_distance = -1.0f;
            s_last_measurement_time = now;
            return s_last_distance;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    // Calculate distance
    if (s_echo_end > s_echo_start) {
        uint32_t pulse_us = s_echo_end - s_echo_start;
        s_last_distance = (pulse_us * SOUND_SPEED) / 2.0f;
        
        // Basic validation
        if (s_last_distance < 2.0f || s_last_distance > 400.0f) {
            s_last_distance = -1.0f;
        }
    } else {
        s_last_distance = -1.0f;
    }
    
    s_last_measurement_time = now;
    return s_last_distance;
}