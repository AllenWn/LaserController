#pragma once

#include <stdint.h>

#include "driver/i2c.h"
#include "esp_err.h"

typedef struct
{
  i2c_port_t port;
  uint8_t addr_7bit;
} tlc59116_i2c_t;

typedef enum
{
  TLC59116_REG_MODE1 = 0x00,
  TLC59116_REG_MODE2 = 0x01,
  TLC59116_REG_PWM0 = 0x02,
  TLC59116_REG_GRPPWM = 0x12,
  TLC59116_REG_GRPFREQ = 0x13,
  TLC59116_REG_LEDOUT0 = 0x14,
  TLC59116_REG_LEDOUT1 = 0x15,
  TLC59116_REG_LEDOUT2 = 0x16,
  TLC59116_REG_LEDOUT3 = 0x17,
} tlc59116_reg_t;

esp_err_t tlc59116_i2c_init(tlc59116_i2c_t *dev, i2c_port_t port, uint8_t addr_7bit);
esp_err_t tlc59116_i2c_write_reg8(const tlc59116_i2c_t *dev, tlc59116_reg_t reg, uint8_t value);
esp_err_t tlc59116_i2c_write_block(const tlc59116_i2c_t *dev, tlc59116_reg_t reg, const uint8_t *data, size_t len);
esp_err_t tlc59116_i2c_all_off(const tlc59116_i2c_t *dev);
esp_err_t tlc59116_i2c_set_pwm(const tlc59116_i2c_t *dev, uint8_t channel, uint8_t pwm);
