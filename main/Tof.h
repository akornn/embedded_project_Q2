#pragma once
#include <stdint.h>
#include "esp_err.h"

// Minimal config: just pins
typedef struct {
    int sda_gpio;
    int scl_gpio;
    int buzzer_gpio;
} tof_config_t;

// Init sensor + GPIO + create tasks
esp_err_t tof_init_and_start(const tof_config_t *cfg);

// Read latest distance (in mm)
esp_err_t tof_get_latest_mm(uint16_t *out_mm);
