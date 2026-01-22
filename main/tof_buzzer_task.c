#include "tof.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "TOF_BUZZER"

/* thresholds in millimeters */
#define TOF_MAX_RANGE_MM   500
#define TOF_FAST_MM        150
#define TOF_CONTINUOUS_MM   80

/* example buzzer hooks (replace with real ones) */
static void buzzer_on(void)  { /* gpio set high */ }
static void buzzer_off(void) { /* gpio set low */ }

void tof_buzzer_task(void *pv)
{
    (void)pv;

    while (1) {
        uint16_t dist = tof_get_latest_mm();

        if (dist == 0 || dist > TOF_MAX_RANGE_MM) {
            buzzer_off();
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        ESP_LOGI(TAG, "Distance: %d mm", dist);

        /* Continuous beep when very close */
        if (dist <= TOF_CONTINUOUS_MM) {
            buzzer_on();
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        /* Beep period scales with distance */
        uint32_t period_ms;

        if (dist <= TOF_FAST_MM) {
            period_ms = 120;   // fast
        } else {
            period_ms = 300;   // slow
        }

        buzzer_on();
        vTaskDelay(pdMS_TO_TICKS(40));
        buzzer_off();
        vTaskDelay(pdMS_TO_TICKS(period_ms));
    }
}

void tof_buzzer_task_start(void)
{
    xTaskCreate(
        tof_buzzer_task,
        "tof_buzzer_task",
        2048,
        NULL,
        5,
        NULL
    );
}
