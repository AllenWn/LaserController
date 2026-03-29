#pragma once

#include <stdint.h>

#include "driver/i2c.h"
#include "esp_err.h"

typedef struct
{
  i2c_port_t port;
  uint8_t addr_7bit;
} stusb4500_i2c_t;

esp_err_t stusb4500_i2c_init(stusb4500_i2c_t *dev, i2c_port_t port, uint8_t addr_7bit);
esp_err_t stusb4500_i2c_probe(const stusb4500_i2c_t *dev);
esp_err_t stusb4500_i2c_read_reg8(const stusb4500_i2c_t *dev, uint8_t reg, uint8_t *value);
esp_err_t stusb4500_i2c_write_reg8(const stusb4500_i2c_t *dev, uint8_t reg, uint8_t value);
