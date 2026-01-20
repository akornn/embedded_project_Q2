#include "tof.h"
#include "esp_err.h"

void app_main(void) // Application entry point
{
    tof_config_t cfg = {
        .sda_gpio = 21,
        .scl_gpio = 22,
        .buzzer_gpio = 25
    };

    ESP_ERROR_CHECK(tof_init_and_start(&cfg));

    // lowkey just an example of reading the distance periodically, you can change it if needed 
}
