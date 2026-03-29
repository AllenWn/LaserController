#include "drivers/tlc59116_i2c.h"

#include "freertos/FreeRTOS.h"

static inline uint8_t addr8_w(uint8_t a7) { return (uint8_t)((a7 << 1) | 0u); }

esp_err_t tlc59116_i2c_init(tlc59116_i2c_t *dev, i2c_port_t port, uint8_t addr_7bit)
{
  if (!dev)
  {
    return ESP_ERR_INVALID_ARG;
  }
  dev->port = port;
  dev->addr_7bit = addr_7bit;
  return ESP_OK;
}

esp_err_t tlc59116_i2c_write_reg8(const tlc59116_i2c_t *dev, tlc59116_reg_t reg, uint8_t value)
{
  if (!dev)
  {
    return ESP_ERR_INVALID_ARG;
  }

  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  esp_err_t err = i2c_master_start(cmd);
  if (err != ESP_OK)
    goto out;
  err = i2c_master_write_byte(cmd, addr8_w(dev->addr_7bit), true);
  if (err != ESP_OK)
    goto out;
  err = i2c_master_write_byte(cmd, (uint8_t)reg, true);
  if (err != ESP_OK)
    goto out;
  err = i2c_master_write_byte(cmd, value, true);
  if (err != ESP_OK)
    goto out;
  err = i2c_master_stop(cmd);
  if (err != ESP_OK)
    goto out;
  err = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(50));
out:
  i2c_cmd_link_delete(cmd);
  return err;
}

esp_err_t tlc59116_i2c_write_block(const tlc59116_i2c_t *dev, tlc59116_reg_t reg, const uint8_t *data, size_t len)
{
  if (!dev || (!data && len != 0u))
  {
    return ESP_ERR_INVALID_ARG;
  }

  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  esp_err_t err = i2c_master_start(cmd);
  if (err != ESP_OK)
    goto out;
  err = i2c_master_write_byte(cmd, addr8_w(dev->addr_7bit), true);
  if (err != ESP_OK)
    goto out;
  err = i2c_master_write_byte(cmd, (uint8_t)reg, true);
  if (err != ESP_OK)
    goto out;
  if (len > 0u)
  {
    err = i2c_master_write(cmd, (uint8_t *)data, len, true);
    if (err != ESP_OK)
      goto out;
  }
  err = i2c_master_stop(cmd);
  if (err != ESP_OK)
    goto out;
  err = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(50));
out:
  i2c_cmd_link_delete(cmd);
  return err;
}

esp_err_t tlc59116_i2c_all_off(const tlc59116_i2c_t *dev)
{
  if (!dev)
  {
    return ESP_ERR_INVALID_ARG;
  }

  const uint8_t ledout_off[4] = {0x00, 0x00, 0x00, 0x00};
  esp_err_t err = tlc59116_i2c_write_reg8(dev, TLC59116_REG_MODE1, 0x00);
  if (err != ESP_OK)
    return err;
  err = tlc59116_i2c_write_reg8(dev, TLC59116_REG_MODE2, 0x00);
  if (err != ESP_OK)
    return err;
  return tlc59116_i2c_write_block(dev, TLC59116_REG_LEDOUT0, ledout_off, sizeof(ledout_off));
}

esp_err_t tlc59116_i2c_set_pwm(const tlc59116_i2c_t *dev, uint8_t channel, uint8_t pwm)
{
  if (!dev || channel >= 16u)
  {
    return ESP_ERR_INVALID_ARG;
  }
  return tlc59116_i2c_write_reg8(dev, (tlc59116_reg_t)(TLC59116_REG_PWM0 + channel), pwm);
}
