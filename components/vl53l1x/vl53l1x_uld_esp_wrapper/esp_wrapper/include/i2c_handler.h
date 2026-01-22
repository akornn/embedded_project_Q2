#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

// Function pointer typedefs used by vl53l1_platform.c
typedef int8_t (*vl53l1x_write_multi)(uint16_t dev, uint16_t index, uint8_t *pdata, uint32_t count);
typedef int8_t (*vl53l1x_read_multi) (uint16_t dev, uint16_t index, uint8_t *pdata, uint32_t count);
typedef int8_t (*vl53l1x_write_byte) (uint16_t dev, uint16_t index, uint8_t data);
typedef int8_t (*vl53l1x_write_word) (uint16_t dev, uint16_t index, uint16_t data);
typedef int8_t (*vl53l1x_write_dword)(uint16_t dev, uint16_t index, uint32_t data);
typedef int8_t (*vl53l1x_read_byte)  (uint16_t dev, uint16_t index, uint8_t *data);
typedef int8_t (*vl53l1x_read_word)  (uint16_t dev, uint16_t index, uint16_t *data);
typedef int8_t (*vl53l1x_read_dword) (uint16_t dev, uint16_t index, uint32_t *data);

// These globals are defined in vl53l1_platform.c (you uploaded it)
extern vl53l1x_write_multi g_vl53l1x_write_multi_ptr;
extern vl53l1x_read_multi  g_vl53l1x_read_multi_ptr;
extern vl53l1x_write_byte  g_vl53l1x_write_byte_ptr;
extern vl53l1x_write_word  g_vl53l1x_write_word_ptr;
extern vl53l1x_write_dword g_vl53l1x_write_dword_ptr;
extern vl53l1x_read_byte   g_vl53l1x_read_byte_ptr;
extern vl53l1x_read_word   g_vl53l1x_read_word_ptr;
extern vl53l1x_read_dword  g_vl53l1x_read_dword_ptr;

// Call this once after creating the VL53 I2C device handle
void i2c_handler_attach_vl53(i2c_master_dev_handle_t dev_handle);

// Default implementations matching ST platform calls
int8_t i2c_write_multi(uint16_t dev, uint16_t index, uint8_t *pdata, uint32_t count);
int8_t i2c_read_multi(uint16_t dev, uint16_t index, uint8_t *pdata, uint32_t count);

int8_t i2c_write_byte(uint16_t dev, uint16_t index, uint8_t data);
int8_t i2c_write_word(uint16_t dev, uint16_t index, uint16_t data);
int8_t i2c_write_dword(uint16_t dev, uint16_t index, uint32_t data);

int8_t i2c_read_byte(uint16_t dev, uint16_t index, uint8_t *data);
int8_t i2c_read_word(uint16_t dev, uint16_t index, uint16_t *data);
int8_t i2c_read_dword(uint16_t dev, uint16_t index, uint32_t *data);