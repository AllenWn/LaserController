#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c.h"
#include "esp_err.h"

// Minimal DAC80502 I2C driver (register read/write).
// Datasheet (TI): DACx0502 family uses:
// - Command byte selecting register (0x00..0x09)
// - Two data bytes (MSDB, LSDB)

typedef struct
{
  i2c_port_t port;
  uint8_t addr_7bit;
} dac80502_i2c_t;

typedef enum
{
  DAC80502_REG_NOOP = 0x00,
  DAC80502_REG_DEVID = 0x01,
  DAC80502_REG_SYNC = 0x02,
  DAC80502_REG_CONFIG = 0x03,
  DAC80502_REG_GAIN = 0x04,
  DAC80502_REG_TRIGGER = 0x05,
  DAC80502_REG_BRDCAST = 0x06,
  DAC80502_REG_STATUS = 0x07,
  DAC80502_REG_DAC_A_DATA = 0x08,
  DAC80502_REG_DAC_B_DATA = 0x09,
} dac80502_reg_t;

esp_err_t dac80502_i2c_init(dac80502_i2c_t *dev, i2c_port_t port, uint8_t addr_7bit);
esp_err_t dac80502_i2c_write_reg(const dac80502_i2c_t *dev, dac80502_reg_t reg, uint16_t value);
esp_err_t dac80502_i2c_read_reg(const dac80502_i2c_t *dev, dac80502_reg_t reg, uint16_t *out_value);

