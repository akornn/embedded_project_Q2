#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

// Simple config struct so main/task stays clean
typedef struct {
    gpio_num_t sda_io;
    gpio_num_t scl_io;
    uint32_t   i2c_clk_hz;     // e.g. 400000
    uint8_t    i2c_addr;       // usually 0x3C
    uint8_t    width;          // 128
    uint8_t    height;         // 64
} oled_ssd1306_cfg_t;

// Opaque handle
typedef struct oled_ssd1306 oled_ssd1306_t;

// Life-cycle
esp_err_t oled_ssd1306_init(oled_ssd1306_t **out, const oled_ssd1306_cfg_t *cfg);
void      oled_ssd1306_deinit(oled_ssd1306_t *oled);

// Drawing
esp_err_t oled_ssd1306_clear(oled_ssd1306_t *oled);
esp_err_t oled_ssd1306_draw_text(oled_ssd1306_t *oled, int x, int page_y, const char *text);
// page_y is in "pages" (0..7 for 64px height), each page = 8 pixels height
esp_err_t oled_ssd1306_show(oled_ssd1306_t *oled);