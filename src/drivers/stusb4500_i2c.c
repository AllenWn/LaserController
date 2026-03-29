#include "drivers/stusb4500_i2c.h"

#include <string.h>

#include "driver/i2c.h"

#ifndef STUSB4500_I2C_TIMEOUT_MS
#define STUSB4500_I2C_TIMEOUT_MS 50
#endif

esp_err_t stusb4500_i2c_init(stusb4500_i2c_t *dev, i2c_port_t port, uint8_t addr_7bit)
{
  if (!dev)
  {
    return ESP_ERR_INVALID_ARG;
  }

  memset(dev, 0, sizeof(*dev));
  dev->port = port;
  dev->addr_7bit = addr_7bit;
  return ESP_OK;
}

esp_err_t stusb4500_i2c_probe(const stusb4500_i2c_t *dev)
{
  if (!dev)
  {
    return ESP_ERR_INVALID_ARG;
  }

  const TickType_t timeout = pdMS_TO_TICKS(STUSB4500_I2C_TIMEOUT_MS);
  return i2c_master_write_to_device(dev->port, dev->addr_7bit, NULL, 0, timeout);
}

esp_err_t stusb4500_i2c_read_reg8(const stusb4500_i2c_t *dev, uint8_t reg, uint8_t *value)
{
  if (!dev || !value)
  {
    return ESP_ERR_INVALID_ARG;
  }

  const TickType_t timeout = pdMS_TO_TICKS(STUSB4500_I2C_TIMEOUT_MS);
  return i2c_master_write_read_device(dev->port, dev->addr_7bit, &reg, 1, value, 1, timeout);
}

esp_err_t stusb4500_i2c_write_reg8(const stusb4500_i2c_t *dev, uint8_t reg, uint8_t value)
{
  if (!dev)
  {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t buf[2] = {reg, value};
  const TickType_t timeout = pdMS_TO_TICKS(STUSB4500_I2C_TIMEOUT_MS);
  return i2c_master_write_to_device(dev->port, dev->addr_7bit, buf, sizeof(buf), timeout);
}
