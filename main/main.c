#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "gyro.h"
#include "oled_ssd1306.h"
#include "freertos/queue.h"
#include "tof_ultra_task.h"

static const char *TAG = "MAIN_APP";
void oled_task(void *pvParameters)
{
    i2c_master_bus_handle_t bus =
        (i2c_master_bus_handle_t)pvParameters;

    oled_ssd1306_t *oled;

    oled_ssd1306_cfg_t cfg = {
        .width = 128,
        .height = 64,
        .i2c_addr = 0x3C,
        .i2c_clk_hz = 100000,
    };

    ESP_ERROR_CHECK(oled_ssd1306_init(&oled, bus, &cfg));

    while (1)
    {
        oled_ssd1306_clear(oled);
        oled_ssd1306_draw_text(oled, 0, 2, "Group 6 | Iron Man");
        oled_ssd1306_show(oled);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    // 1. Create the shared I2C Bus Configuration
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_1,
        .scl_io_num = 21,
        .sda_io_num = 22,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle;

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));
    ESP_LOGI(TAG, "Shared I2C bus initialized on GPIO 21/22 (I2C_NUM_1)");

    // 3. Start Tasks - Pass 'bus_handle' to any task that needs I2C
    xTaskCreatePinnedToCore(imu_task, "imu_task", 4096, (void *)bus_handle, 5, NULL, 0);
    xTaskCreatePinnedToCore(oled_task, "oled_task", 4096, (void *)bus_handle, 4, NULL, 1);

    wifi_init_sta();
    xTaskCreatePinnedToCore(mqtt_task, "mqtt_task", 4096, NULL, 3, NULL, 1);


    tof_ultra_task_start();
}