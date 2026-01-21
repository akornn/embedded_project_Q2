#include "oled_ssd1306.h"

#include <string.h>
#include <stdlib.h>

#include "esp_log.h"
#include "driver/i2c_master.h"

static const char *TAG = "oled_ssd1306";

struct oled_ssd1306 {
    oled_ssd1306_cfg_t cfg;

    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t dev;

    uint8_t *fb;          // framebuffer: width * (height/8)
    size_t   fb_len;
};

/* ----------------------------- 5x7 font ----------------------------- */
/* Minimal ASCII 32..127 font (5 columns each). Enough for numbers/text.
   If you already have a font table in your project, you can replace this. */
static const uint8_t font5x7[][5] = {
    // 32 ' '
    {0x00,0x00,0x00,0x00,0x00},
    // 33 '!'
    {0x00,0x00,0x5F,0x00,0x00},
    // 34 '"'
    {0x00,0x07,0x00,0x07,0x00},
    // 35 '#'
    {0x14,0x7F,0x14,0x7F,0x14},
    // 36 '$'
    {0x24,0x2A,0x7F,0x2A,0x12},
    // 37 '%'
    {0x23,0x13,0x08,0x64,0x62},
    // 38 '&'
    {0x36,0x49,0x55,0x22,0x50},
    // 39 '''
    {0x00,0x05,0x03,0x00,0x00},
    // 40 '('
    {0x00,0x1C,0x22,0x41,0x00},
    // 41 ')'
    {0x00,0x41,0x22,0x1C,0x00},
    // 42 '*'
    {0x14,0x08,0x3E,0x08,0x14},
    // 43 '+'
    {0x08,0x08,0x3E,0x08,0x08},
    // 44 ','
    {0x00,0x50,0x30,0x00,0x00},
    // 45 '-'
    {0x08,0x08,0x08,0x08,0x08},
    // 46 '.'
    {0x00,0x60,0x60,0x00,0x00},
    // 47 '/'
    {0x20,0x10,0x08,0x04,0x02},
    // 48 '0'
    {0x3E,0x51,0x49,0x45,0x3E},
    // 49 '1'
    {0x00,0x42,0x7F,0x40,0x00},
    // 50 '2'
    {0x42,0x61,0x51,0x49,0x46},
    // 51 '3'
    {0x21,0x41,0x45,0x4B,0x31},
    // 52 '4'
    {0x18,0x14,0x12,0x7F,0x10},
    // 53 '5'
    {0x27,0x45,0x45,0x45,0x39},
    // 54 '6'
    {0x3C,0x4A,0x49,0x49,0x30},
    // 55 '7'
    {0x01,0x71,0x09,0x05,0x03},
    // 56 '8'
    {0x36,0x49,0x49,0x49,0x36},
    // 57 '9'
    {0x06,0x49,0x49,0x29,0x1E},
    // 58 ':'
    {0x00,0x36,0x36,0x00,0x00},
    // 59 ';'
    {0x00,0x56,0x36,0x00,0x00},
    // 60 '<'
    {0x08,0x14,0x22,0x41,0x00},
    // 61 '='
    {0x14,0x14,0x14,0x14,0x14},
    // 62 '>'
    {0x00,0x41,0x22,0x14,0x08},
    // 63 '?'
    {0x02,0x01,0x51,0x09,0x06},
    // 64 '@'
    {0x32,0x49,0x79,0x41,0x3E},
    // 65 'A'
    {0x7E,0x11,0x11,0x11,0x7E},
    // 66 'B'
    {0x7F,0x49,0x49,0x49,0x36},
    // 67 'C'
    {0x3E,0x41,0x41,0x41,0x22},
    // 68 'D'
    {0x7F,0x41,0x41,0x22,0x1C},
    // 69 'E'
    {0x7F,0x49,0x49,0x49,0x41},
    // 70 'F'
    {0x7F,0x09,0x09,0x09,0x01},
    // 71 'G'
    {0x3E,0x41,0x49,0x49,0x7A},
    // 72 'H'
    {0x7F,0x08,0x08,0x08,0x7F},
    // 73 'I'
    {0x00,0x41,0x7F,0x41,0x00},
    // 74 'J'
    {0x20,0x40,0x41,0x3F,0x01},
    // 75 'K'
    {0x7F,0x08,0x14,0x22,0x41},
    // 76 'L'
    {0x7F,0x40,0x40,0x40,0x40},
    // 77 'M'
    {0x7F,0x02,0x0C,0x02,0x7F},
    // 78 'N'
    {0x7F,0x04,0x08,0x10,0x7F},
    // 79 'O'
    {0x3E,0x41,0x41,0x41,0x3E},
    // 80 'P'
    {0x7F,0x09,0x09,0x09,0x06},
    // 81 'Q'
    {0x3E,0x41,0x51,0x21,0x5E},
    // 82 'R'
    {0x7F,0x09,0x19,0x29,0x46},
    // 83 'S'
    {0x46,0x49,0x49,0x49,0x31},
    // 84 'T'
    {0x01,0x01,0x7F,0x01,0x01},
    // 85 'U'
    {0x3F,0x40,0x40,0x40,0x3F},
    // 86 'V'
    {0x1F,0x20,0x40,0x20,0x1F},
    // 87 'W'
    {0x7F,0x20,0x18,0x20,0x7F},
    // 88 'X'
    {0x63,0x14,0x08,0x14,0x63},
    // 89 'Y'
    {0x03,0x04,0x78,0x04,0x03},
    // 90 'Z'
    {0x61,0x51,0x49,0x45,0x43},
    // 91 '['
    {0x00,0x7F,0x41,0x41,0x00},
    // 92 '\'
    {0x02,0x04,0x08,0x10,0x20},
    // 93 ']'
    {0x00,0x41,0x41,0x7F,0x00},
    // 94 '^'
    {0x04,0x02,0x01,0x02,0x04},
    // 95 '_'
    {0x40,0x40,0x40,0x40,0x40},
    // 96 '`'
    {0x00,0x01,0x02,0x04,0x00},
    // 97 'a'
    {0x20,0x54,0x54,0x54,0x78},
    // 98 'b'
    {0x7F,0x48,0x44,0x44,0x38},
    // 99 'c'
    {0x38,0x44,0x44,0x44,0x20},
    // 100 'd'
    {0x38,0x44,0x44,0x48,0x7F},
    // 101 'e'
    {0x38,0x54,0x54,0x54,0x18},
    // 102 'f'
    {0x08,0x7E,0x09,0x01,0x02},
    // 103 'g'
    {0x0C,0x52,0x52,0x52,0x3E},
    // 104 'h'
    {0x7F,0x08,0x04,0x04,0x78},
    // 105 'i'
    {0x00,0x44,0x7D,0x40,0x00},
    // 106 'j'
    {0x20,0x40,0x44,0x3D,0x00},
    // 107 'k'
    {0x7F,0x10,0x28,0x44,0x00},
    // 108 'l'
    {0x00,0x41,0x7F,0x40,0x00},
    // 109 'm'
    {0x7C,0x04,0x18,0x04,0x78},
    // 110 'n'
    {0x7C,0x08,0x04,0x04,0x78},
    // 111 'o'
    {0x38,0x44,0x44,0x44,0x38},
    // 112 'p'
    {0x7C,0x14,0x14,0x14,0x08},
    // 113 'q'
    {0x08,0x14,0x14,0x18,0x7C},
    // 114 'r'
    {0x7C,0x08,0x04,0x04,0x08},
    // 115 's'
    {0x48,0x54,0x54,0x54,0x20},
    // 116 't'
    {0x04,0x3F,0x44,0x40,0x20},
    // 117 'u'
    {0x3C,0x40,0x40,0x20,0x7C},
    // 118 'v'
    {0x1C,0x20,0x40,0x20,0x1C},
    // 119 'w'
    {0x3C,0x40,0x30,0x40,0x3C},
    // 120 'x'
    {0x44,0x28,0x10,0x28,0x44},
    // 121 'y'
    {0x0C,0x50,0x50,0x50,0x3C},
    // 122 'z'
    {0x44,0x64,0x54,0x4C,0x44},
    // 123 '{'
    {0x00,0x08,0x36,0x41,0x00},
    // 124 '|'
    {0x00,0x00,0x7F,0x00,0x00},
    // 125 '}'
    {0x00,0x41,0x36,0x08,0x00},
    // 126 '~'
    {0x08,0x04,0x08,0x10,0x08},
};

static inline const uint8_t *glyph_for(char c)
{
    if (c < 32 || c > 126) c = '?';
    return font5x7[(int)c - 32];
}

/* -------------------------- I2C + SSD1306 --------------------------- */

static esp_err_t ssd1306_write_cmd(oled_ssd1306_t *o, uint8_t cmd)
{
    // Control byte 0x00 = command
    uint8_t buf[2] = {0x00, cmd};
    return i2c_master_transmit(o->dev, buf, sizeof(buf), -1);
}

static esp_err_t ssd1306_write_data(oled_ssd1306_t *o, const uint8_t *data, size_t len)
{
    // Control byte 0x40 = data
    // For simplicity, send in chunks to avoid huge stack usage
    const size_t CHUNK = 128;
    uint8_t tmp[1 + CHUNK];
    tmp[0] = 0x40;

    size_t off = 0;
    while (off < len) {
        size_t n = (len - off > CHUNK) ? CHUNK : (len - off);
        memcpy(&tmp[1], &data[off], n);
        esp_err_t err = i2c_master_transmit(o->dev, tmp, 1 + n, -1);
        if (err != ESP_OK) return err;
        off += n;
    }
    return ESP_OK;
}

static void fb_set_byte(oled_ssd1306_t *o, int x, int page, uint8_t v)
{
    if (x < 0 || x >= o->cfg.width) return;
    if (page < 0 || page >= (o->cfg.height / 8)) return;
    o->fb[page * o->cfg.width + x] = v;
}

esp_err_t oled_ssd1306_init(oled_ssd1306_t **out, const oled_ssd1306_cfg_t *cfg)
{
    if (!out || !cfg) return ESP_ERR_INVALID_ARG;

    oled_ssd1306_t *o = calloc(1, sizeof(*o));
    if (!o) return ESP_ERR_NO_MEM;

    o->cfg = *cfg;
    o->fb_len = (size_t)o->cfg.width * (o->cfg.height / 8);
    o->fb = calloc(1, o->fb_len);
    if (!o->fb) {
        free(o);
        return ESP_ERR_NO_MEM;
    }

    // Create I2C bus (ESP-IDF v5 style)
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,                 // ESP32 has I2C0/I2C1; keep 0 unless you need 1
        .sda_io_num = o->cfg.sda_io,
        .scl_io_num = o->cfg.scl_io,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .flags.enable_internal_pullup = true,
    };

    ESP_LOGI(TAG, "Creating I2C bus SDA=%d SCL=%d", o->cfg.sda_io, o->cfg.scl_io);
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &o->bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
        oled_ssd1306_deinit(o);
        return err;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = o->cfg.i2c_addr,
        .scl_speed_hz    = o->cfg.i2c_clk_hz,
    };

    err = i2c_master_bus_add_device(o->bus, &dev_cfg, &o->dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_master_bus_add_device failed: %s", esp_err_to_name(err));
        oled_ssd1306_deinit(o);
        return err;
    }

    // SSD1306 init sequence for 128x64
    // (works for most 0x3C modules)
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0xAE)); // display off
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0xD5)); // set display clock
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0x80));
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0xA8)); // multiplex
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, o->cfg.height - 1));
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0xD3)); // display offset
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0x00));
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0x40)); // start line
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0x8D)); // charge pump
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0x14));
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0x20)); // memory mode
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0x00)); // horizontal addressing
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0xA1)); // seg remap
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0xC8)); // COM scan dec
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0xDA)); // com pins
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, (o->cfg.height == 64) ? 0x12 : 0x02));
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0x81)); // contrast
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0x7F));
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0xD9)); // precharge
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0xF1));
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0xDB)); // vcom detect
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0x40));
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0xA4)); // resume RAM display
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0xA6)); // normal display
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0xAF)); // display on

    // Clear and show
    ESP_ERROR_CHECK(oled_ssd1306_clear(o));
    ESP_ERROR_CHECK(oled_ssd1306_show(o));

    *out = o;
    ESP_LOGI(TAG, "SSD1306 initialized OK");
    return ESP_OK;
}

void oled_ssd1306_deinit(oled_ssd1306_t *o)
{
    if (!o) return;

    if (o->dev && o->bus) {
        i2c_master_bus_rm_device(o->dev);
        o->dev = NULL;
    }
    if (o->bus) {
        i2c_del_master_bus(o->bus);
        o->bus = NULL;
    }
    free(o->fb);
    free(o);
}

esp_err_t oled_ssd1306_clear(oled_ssd1306_t *o)
{
    if (!o || !o->fb) return ESP_ERR_INVALID_ARG;
    memset(o->fb, 0x00, o->fb_len);
    return ESP_OK;
}

esp_err_t oled_ssd1306_draw_text(oled_ssd1306_t *o, int x, int page_y, const char *text)
{
    if (!o || !text) return ESP_ERR_INVALID_ARG;
    if (page_y < 0 || page_y >= (o->cfg.height / 8)) return ESP_ERR_INVALID_ARG;

    int cursor = x;

    while (*text) {
        const uint8_t *g = glyph_for(*text++);
        // 5 columns + 1 spacing
        for (int col = 0; col < 5; col++) {
            fb_set_byte(o, cursor++, page_y, g[col]);
        }
        fb_set_byte(o, cursor++, page_y, 0x00);
        if (cursor >= o->cfg.width) break;
    }
    return ESP_OK;
}

esp_err_t oled_ssd1306_show(oled_ssd1306_t *o)
{
    if (!o || !o->fb) return ESP_ERR_INVALID_ARG;

    // set column + page ranges then push buffer
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0x21)); // column addr
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0x00));
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, o->cfg.width - 1));

    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0x22)); // page addr
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, 0x00));
    ESP_ERROR_CHECK(ssd1306_write_cmd(o, (o->cfg.height / 8) - 1));

    return ssd1306_write_data(o, o->fb, o->fb_len);
}