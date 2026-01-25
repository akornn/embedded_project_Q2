#include "i2c_bus.h"
#include "esp_log.h"

static const char *TAG = "I2C_BUS";
static bool s_i2c_initialized = false;

esp_err_t i2c_bus_init(void)
{
    if (s_i2c_initialized) {
        ESP_LOGW(TAG, "I2C bus already initialized");
        return ESP_OK;
    }
    
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    ESP_LOGI(TAG, "Initializing I2C bus on GPIO%d (SDA), GPIO%d (SCL) at %d Hz", 
             I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, I2C_MASTER_FREQ_HZ);
    
    esp_err_t err = i2c_param_config(I2C_MASTER_PORT, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config failed: %s", esp_err_to_name(err));
        return err;
    }
    
    err = i2c_driver_install(I2C_MASTER_PORT, conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install failed: %s", esp_err_to_name(err));
        return err;
    }
    
    s_i2c_initialized = true;
    ESP_LOGI(TAG, "I2C bus initialized successfully");
    return ESP_OK;
}

bool i2c_bus_is_initialized(void)
{
    return s_i2c_initialized;
}

i2c_port_t i2c_bus_get_port(void)
{
    return I2C_MASTER_PORT;
}