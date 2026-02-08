#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"

typedef struct {
    gpio_num_t pir_gpio;
    gpio_num_t led_gpio;

    uint32_t   poll_ms;
    uint32_t   hold_ms;

    bool       pir_active_high;   // HC-SR501 usually true
    bool       led_active_high;   // set true if LED turns ON when GPIO=1, false if ON when GPIO=0
} pir_led_cfg_t;

void pir_led_init(const pir_led_cfg_t *cfg);
void pir_led_task(void *pvParameters);