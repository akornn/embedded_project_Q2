#include "pir_led.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

static const char *TAG = "PIR_LED";

static pir_led_cfg_t s_cfg;
static bool s_inited = false;

static inline void led_write(bool on)
{
    // If LED is active-high: ON=1 OFF=0
    // If LED is active-low : ON=0 OFF=1
    int level = on ? (s_cfg.led_active_high ? 1 : 0)
                   : (s_cfg.led_active_high ? 0 : 1);
    gpio_set_level(s_cfg.led_gpio, level);
}

void pir_led_init(const pir_led_cfg_t *cfg)
{
    if (!cfg) {
        ESP_LOGE(TAG, "Config is NULL");
        return;
    }

    s_cfg = *cfg;
    if (s_cfg.poll_ms == 0) s_cfg.poll_ms = 100;

    // PIR input
    gpio_config_t pir_conf = {
        .pin_bit_mask = 1ULL << s_cfg.pir_gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&pir_conf));

    // LED output
    gpio_config_t led_conf = {
        .pin_bit_mask = 1ULL << s_cfg.led_gpio,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&led_conf));

    // Force OFF at startup (using correct polarity)
    led_write(false);

    s_inited = true;

    ESP_LOGI(TAG, "Init OK. PIR=%d LED=%d poll=%ums hold=%ums pir_active_high=%d led_active_high=%d",
             (int)s_cfg.pir_gpio, (int)s_cfg.led_gpio,
             (unsigned)s_cfg.poll_ms, (unsigned)s_cfg.hold_ms,
             (int)s_cfg.pir_active_high, (int)s_cfg.led_active_high);
}

void pir_led_task(void *pvParameters)
{
    if (!s_inited) {
        pir_led_init((pir_led_cfg_t *)pvParameters);
    }

    int64_t last_motion_us = 0;
    bool led_state = false;

    while (1) {
        int pir_level = gpio_get_level(s_cfg.pir_gpio);
        bool motion = s_cfg.pir_active_high ? (pir_level == 1) : (pir_level == 0);

        if (motion) last_motion_us = esp_timer_get_time();

        bool want_led_on = false;
        if (s_cfg.hold_ms == 0) {
            want_led_on = motion;
        } else {
            int64_t now = esp_timer_get_time();
            want_led_on = motion ||
                          (last_motion_us &&
                           (now - last_motion_us) < (int64_t)s_cfg.hold_ms * 1000);
        }

        if (want_led_on != led_state) {
            led_state = want_led_on;
            led_write(led_state);

            if (led_state) ESP_LOGI(TAG, "Motion detected -> LED ON");
            else           ESP_LOGI(TAG, "No motion -> LED OFF");
        }

        vTaskDelay(pdMS_TO_TICKS(s_cfg.poll_ms));
    }
}