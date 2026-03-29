#include "drivers/mcp23017_i2c.h"

#include "freertos/FreeRTOS.h"

static inline uint8_t addr8_w(uint8_t a7) { return (uint8_t)((a7 << 1) | 0u); }
static inline uint8_t addr8_r(uint8_t a7) { return (uint8_t)((a7 << 1) | 1u); }

esp_err_t mcp23017_i2c_init(mcp23017_i2c_t *dev, i2c_port_t port, uint8_t addr_7bit)
{
  if (!dev)
  {
    return ESP_ERR_INVALID_ARG;
  }
  dev->port = port;
  dev->addr_7bit = addr_7bit;
  return ESP_OK;
}

esp_err_t mcp23017_i2c_write_reg8(const mcp23017_i2c_t *dev, mcp23017_reg_t reg, uint8_t value)
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

esp_err_t mcp23017_i2c_read_reg8(const mcp23017_i2c_t *dev, mcp23017_reg_t reg, uint8_t *out_value)
{
  if (!dev || !out_value)
  {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t value = 0;

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

  err = i2c_master_start(cmd);
  if (err != ESP_OK)
    goto out;

  err = i2c_master_write_byte(cmd, addr8_r(dev->addr_7bit), true);
  if (err != ESP_OK)
    goto out;

  err = i2c_master_read_byte(cmd, &value, I2C_MASTER_NACK);
  if (err != ESP_OK)
    goto out;

  err = i2c_master_stop(cmd);
  if (err != ESP_OK)
    goto out;

  err = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(50));
  if (err != ESP_OK)
    goto out;

  *out_value = value;

out:
  i2c_cmd_link_delete(cmd);
  return err;
}
