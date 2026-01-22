#ifndef TOF_H
#define TOF_H

#include "esp_err.h"
#include "driver/i2c_master.h"

typedef struct {
    int buzzer_gpio;
} tof_config_t;

// UPDATED: Added i2c_master_bus_handle_t to match tof.c
esp_err_t tof_init_and_start(i2c_master_bus_handle_t bus_handle, const tof_config_t *cfg);

esp_err_t tof_get_latest_mm(uint16_t *out_mm);

#endif