#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

/* ===================== OLED CONFIG ===================== */
typedef struct {
    uint8_t width;          // e.g. 128
    uint8_t height;         // e.g. 64
    uint8_t i2c_addr;       // usually 0x3C
    uint32_t i2c_clk_hz;    // 100000 or 400000
} oled_ssd1306_cfg_t;

/* Forward declaration (opaque handle) */
typedef struct oled_ssd1306 oled_ssd1306_t;

/* ===================== DRIVER API ===================== */

/**
 * @brief Initialize SSD1306 OLED on a shared I2C bus
 *
 * @param out   Pointer to OLED handle
 * @param bus   Shared I2C master bus handle (from app_main)
 * @param cfg   OLED configuration
 */
esp_err_t oled_ssd1306_init(oled_ssd1306_t **out,
                            i2c_master_bus_handle_t bus,
                            const oled_ssd1306_cfg_t *cfg);

/**
 * @brief Deinitialize OLED (removes device from bus)
 */
void oled_ssd1306_deinit(oled_ssd1306_t *o);

/**
 * @brief Clear framebuffer
 */
esp_err_t oled_ssd1306_clear(oled_ssd1306_t *o);

/**
 * @brief Draw ASCII text at column x and page y (page = y/8)
 */
esp_err_t oled_ssd1306_draw_text(oled_ssd1306_t *o,
                                 int x,
                                 int page_y,
                                 const char *text);

/**
 * @brief Flush framebuffer to display
 */
esp_err_t oled_ssd1306_show(oled_ssd1306_t *o);

/* ===================== FREERTOS TASK ===================== */

/**
 * @brief OLED task entry point
 *
 * pvParameters must be i2c_master_bus_handle_t
 */
void oled_task(void *pvParameters);
