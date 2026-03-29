#pragma once

#include <stdint.h>

#include "driver/i2c.h"
#include "esp_err.h"

typedef struct
{
  i2c_port_t port;
  uint8_t addr_7bit;
} drv2605_i2c_t;

typedef enum
{
  DRV2605_REG_STATUS = 0x00,
  DRV2605_REG_MODE = 0x01,
  DRV2605_REG_RTP_INPUT = 0x02,
  DRV2605_REG_LIBRARY_SEL = 0x03,
  DRV2605_REG_WAVESEQ1 = 0x04,
  DRV2605_REG_GO = 0x0C,
} drv2605_reg_t;

esp_err_t drv2605_i2c_init(drv2605_i2c_t *dev, i2c_port_t port, uint8_t addr_7bit);
esp_err_t drv2605_i2c_write_reg8(const drv2605_i2c_t *dev, drv2605_reg_t reg, uint8_t value);
esp_err_t drv2605_i2c_read_reg8(const drv2605_i2c_t *dev, drv2605_reg_t reg, uint8_t *out_value);
esp_err_t drv2605_i2c_enter_standby(const drv2605_i2c_t *dev);
esp_err_t drv2605_i2c_set_rtp(const drv2605_i2c_t *dev, uint8_t value);
