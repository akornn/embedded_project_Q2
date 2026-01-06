#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdlib.h>
#include <time.h>

#define BUZZER_GPIO GPIO_NUM_22
#define BUTTON_GPIO GPIO_NUM_23
#define TAG "DEBUG"

// Simple buzzer pattern function
void play_pattern(int pin, int on_ms, int off_ms, int repeat)
{
    for (int i = 0; i < repeat; i++)
    {
        gpio_set_level(pin, 1); 
        vTaskDelay(pdMS_TO_TICKS(on_ms));

        gpio_set_level(pin, 0); 
        vTaskDelay(pdMS_TO_TICKS(off_ms));
    }
}

void app_main()
{
    srand(time(NULL));

    gpio_config_t buzzer_cfg = {
        .pin_bit_mask = (1ULL << BUZZER_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&buzzer_cfg);

    gpio_config_t button_cfg = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&button_cfg);

    int last_button_state = 0;
    int sensor_timer = 0;

    while (1)
    {
        // --- BUTTON MONITOR ---
        int current_button = gpio_get_level(BUTTON_GPIO);
        if (current_button != last_button_state)
        {
            if (current_button == 1)
            {
                ESP_LOGI(TAG, "BUTTON PRESSED → manual pattern");
                play_pattern(BUZZER_GPIO, 80, 120, 3);
            }
            last_button_state = current_button;
        }

        // --- SENSOR SIMULATION ---
        // Only update every ~1 second
        if (sensor_timer <= 0)
        {
            int sensor_value = rand() % 101;
            ESP_LOGI(TAG, "Sensor value: %d", sensor_value);

            if (sensor_value < 30)
                play_pattern(BUZZER_GPIO, 80, 120, 1);
            else if (sensor_value < 70)
                play_pattern(BUZZER_GPIO, 200, 100, 3);
            else
                play_pattern(BUZZER_GPIO, 100, 50, 10);

            sensor_timer = 1000;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
        sensor_timer -= 10;
    }
}
