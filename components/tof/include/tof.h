#pragma once
#include "driver/i2c_master.h"
#include "esp_err.h"

esp_err_t tof_init(i2c_master_bus_handle_t bus);
uint16_t tof_get_latest_mm(void);
