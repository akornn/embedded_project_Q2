#pragma once
#include <stdbool.h>
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define I2C_MASTER_PORT     I2C_NUM_0
#define I2C_MASTER_SDA_IO   21
#define I2C_MASTER_SCL_IO   22
#define I2C_MASTER_FREQ_HZ  100000

// Initialize the shared I2C bus (call this ONCE in app_main)
esp_err_t i2c_bus_init(void);

// Check if I2C bus is initialized
bool i2c_bus_is_initialized(void);

// Get the I2C port (for direct i2c_master_* function calls)
i2c_port_t i2c_bus_get_port(void);

#ifdef __cplusplus
}
#endif