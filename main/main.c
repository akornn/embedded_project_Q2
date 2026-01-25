#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"      // Fixes the xTaskCreate error
#include "driver/i2c_master.h"  // The NEW driver
#include "esp_log.h"
#include "gyro.h"
#include "tof_ultra_task.h"

static const char *TAG = "MAIN_APP";

void app_main(void) {
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

    wifi_init_sta();

    // 3. Start Tasks - Pass 'bus_handle' to any task that needs I2C
    xTaskCreatePinnedToCore(imu_task, "imu_task", 4096, (void*)bus_handle, 5, NULL, 0);
    xTaskCreatePinnedToCore(mqtt_task, "mqtt_task", 4096, NULL, 3, NULL, 1);
    tof_ultra_task_start();
    // If your ToF task is updated, pass the handle there too:
    // tof_ultra_task_start(bus_handle); 
}