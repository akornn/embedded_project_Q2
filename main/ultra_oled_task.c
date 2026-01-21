// ultra_oled_task.c

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "ultrasonic.h"
#include "oled_ssd1306.h"
#include "ultra_oled_task.h"

// -------- Pins --------
#define ULTRA_TRIG_GPIO   GPIO_NUM_26
#define ULTRA_ECHO_GPIO   GPIO_NUM_27  

#define OLED_SDA_GPIO     GPIO_NUM_21
#define OLED_SCL_GPIO     GPIO_NUM_22

static const char *TAG = "ultra_oled_task";

static void ultra_oled_task(void *pv)
{
    (void)pv;

    // 1) Init OLED
    oled_ssd1306_t *oled = NULL;
    oled_ssd1306_cfg_t oled_cfg = {
        .sda_io     = OLED_SDA_GPIO,
        .scl_io     = OLED_SCL_GPIO,
        .i2c_clk_hz = 400000,
        .i2c_addr   = 0x3C,
        .width      = 128,
        .height     = 64,
    };
    ESP_ERROR_CHECK(oled_ssd1306_init(&oled, &oled_cfg));

    // 2) Init Ultrasonic
    ultrasonic_sensor_t us = {
        .trigger_pin = ULTRA_TRIG_GPIO,
        .echo_pin    = ULTRA_ECHO_GPIO,
    };
    ESP_ERROR_CHECK(ultrasonic_init(&us));

    // Helpful: ensure echo isn't floating
    gpio_set_pull_mode(ULTRA_ECHO_GPIO, GPIO_PULLDOWN_ONLY);

    while (1) {
        uint32_t dist_cm = 0;
        esp_err_t err = ESP_FAIL;

        // Retry a few times (timeouts happen)
        for (int i = 0; i < 3; i++) {
            err = ultrasonic_measure_cm(&us, 200 /* max cm */, &dist_cm);
            if (err == ESP_OK) break;
            vTaskDelay(pdMS_TO_TICKS(60));
        }

        char line1[24];
        char line2[24];

        ESP_ERROR_CHECK(oled_ssd1306_clear(oled));
        ESP_ERROR_CHECK(oled_ssd1306_draw_text(oled, 0, 0, "Ultrasonic"));

        if (err == ESP_OK) {
            snprintf(line1, sizeof(line1), "Dist: %lu cm", (unsigned long)dist_cm);
            snprintf(line2, sizeof(line2), "OK");
            ESP_LOGI(TAG, "Distance: %lu cm", (unsigned long)dist_cm);
        } else {
            snprintf(line1, sizeof(line1), "Dist: ---");
            snprintf(line2, sizeof(line2), "Err:%d", (int)err);
            ESP_LOGE(TAG, "ultrasonic_measure_cm failed: %d", (int)err);
        }

        ESP_ERROR_CHECK(oled_ssd1306_draw_text(oled, 0, 2, line1));
        ESP_ERROR_CHECK(oled_ssd1306_draw_text(oled, 0, 4, line2));
        ESP_ERROR_CHECK(oled_ssd1306_show(oled));

        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void ultra_oled_task_start(void)
{
    xTaskCreate(ultra_oled_task, "ultra_oled_task", 4096, NULL, 5, NULL);
}