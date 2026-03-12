#include "drivers/lsm6dso_spi.h"

#include <string.h>

// LSM6DSO SPI conventions:
// - bit7 = 1 for read, 0 for write
// - bit6 = 1 enables address auto-increment (multi-byte)
static inline uint8_t spi_addr(uint8_t reg, bool read, bool inc)
{
  uint8_t a = reg;
  if (read)
    a |= 0x80;
  if (inc)
    a |= 0x40;
  return a;
}

esp_err_t lsm6dso_spi_init(lsm6dso_spi_t *imu, const lsm6dso_spi_config_t *cfg)
{
  if (!imu || !cfg)
  {
    return ESP_ERR_INVALID_ARG;
  }

  memset(imu, 0, sizeof(*imu));
  imu->host = cfg->host;

  const spi_bus_config_t buscfg = {
      .mosi_io_num = cfg->mosi_gpio,
      .miso_io_num = cfg->miso_gpio,
      .sclk_io_num = cfg->sclk_gpio,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 64,
  };

  // If the bus is already initialized, ESP-IDF returns ESP_ERR_INVALID_STATE.
  esp_err_t err = spi_bus_initialize(cfg->host, &buscfg, SPI_DMA_CH_AUTO);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
  {
    return err;
  }

  const spi_device_interface_config_t devcfg = {
      .clock_speed_hz = cfg->clock_hz,
      .mode = cfg->spi_mode,
      .spics_io_num = cfg->cs_gpio,
      .queue_size = 1,
      // LSM6DSO uses a single wire for SDI; reads are done by clocking out dummy bytes.
      .flags = SPI_DEVICE_HALFDUPLEX,
  };

  return spi_bus_add_device(cfg->host, &devcfg, &imu->dev);
}

esp_err_t lsm6dso_spi_read_reg(const lsm6dso_spi_t *imu, uint8_t reg, uint8_t *out, size_t len)
{
  if (!imu || !imu->dev || !out || len == 0)
  {
    return ESP_ERR_INVALID_ARG;
  }
  if (len > 32)
  {
    return ESP_ERR_INVALID_SIZE;
  }

  // tx: [addr][dummy...], rx: [junk][data...]
  uint8_t tx[1 + 32] = {0};
  uint8_t rx[1 + 32] = {0};
  tx[0] = spi_addr(reg, true, len > 1);

  spi_transaction_t t = {0};
  t.length = (1 + len) * 8;
  t.tx_buffer = tx;
  t.rx_buffer = rx;

  const esp_err_t err = spi_device_transmit(imu->dev, &t);
  if (err != ESP_OK)
  {
    return err;
  }

  memcpy(out, &rx[1], len);
  return ESP_OK;
}

esp_err_t lsm6dso_spi_write_reg(const lsm6dso_spi_t *imu, uint8_t reg, const uint8_t *data, size_t len)
{
  if (!imu || !imu->dev || !data || len == 0)
  {
    return ESP_ERR_INVALID_ARG;
  }
  if (len > 32)
  {
    return ESP_ERR_INVALID_SIZE;
  }

  uint8_t tx[1 + 32] = {0};
  tx[0] = spi_addr(reg, false, len > 1);
  memcpy(&tx[1], data, len);

  spi_transaction_t t = {0};
  t.length = (1 + len) * 8;
  t.tx_buffer = tx;
  return spi_device_transmit(imu->dev, &t);
}

esp_err_t lsm6dso_spi_read_whoami(const lsm6dso_spi_t *imu, uint8_t *out_whoami)
{
  if (!out_whoami)
  {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t v = 0;
  const esp_err_t err = lsm6dso_spi_read_reg(imu, 0x0F, &v, 1);
  if (err != ESP_OK)
  {
    return err;
  }

  *out_whoami = v;
  return ESP_OK;
}

