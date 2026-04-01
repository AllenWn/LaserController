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

  // Avoid zero-length writes here: the legacy IDF I2C driver logs a noisy
  // "null address error" when asked to probe with no payload. Read a known
  // readable register instead so absent devices fail quietly with the returned
  // esp_err_t.
  uint8_t value = 0;
  return stusb4500_i2c_read_reg8(dev, 0x0D, &value);
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

esp_err_t stusb4500_i2c_read_block(const stusb4500_i2c_t *dev, uint8_t reg, uint8_t *buf, size_t len)
{
  if (!dev || !buf || len == 0u)
  {
    return ESP_ERR_INVALID_ARG;
  }

  const TickType_t timeout = pdMS_TO_TICKS(STUSB4500_I2C_TIMEOUT_MS);
  return i2c_master_write_read_device(dev->port, dev->addr_7bit, &reg, 1, buf, len, timeout);
}

esp_err_t stusb4500_i2c_write_block(const stusb4500_i2c_t *dev, uint8_t reg, const uint8_t *buf, size_t len)
{
  if (!dev || !buf || len == 0u)
  {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t tx[1 + 16];
  if (len > 16u)
  {
    return ESP_ERR_INVALID_SIZE;
  }

  tx[0] = reg;
  memcpy(&tx[1], buf, len);

  const TickType_t timeout = pdMS_TO_TICKS(STUSB4500_I2C_TIMEOUT_MS);
  return i2c_master_write_to_device(dev->port, dev->addr_7bit, tx, len + 1u, timeout);
}
