#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/spi_master.h"
#include "esp_err.h"

// Minimal SPI driver for ST LSM6DSO.
// - Does register R/W over SPI
// - Does NOT implement any orientation logic (that belongs in imu_monitor)

typedef struct
{
  spi_host_device_t host;
  spi_device_handle_t dev;
} lsm6dso_spi_t;

typedef struct
{
  spi_host_device_t host;
  int sclk_gpio;
  int mosi_gpio; // IMU_SDI
  int miso_gpio; // IMU_SDO
  int cs_gpio;   // IMU_CS
  int spi_mode;  // LSM6DSO supports mode 0 and 3
  int clock_hz;
} lsm6dso_spi_config_t;

esp_err_t lsm6dso_spi_init(lsm6dso_spi_t *imu, const lsm6dso_spi_config_t *cfg);

esp_err_t lsm6dso_spi_read_reg(const lsm6dso_spi_t *imu, uint8_t reg, uint8_t *out, size_t len);
esp_err_t lsm6dso_spi_write_reg(const lsm6dso_spi_t *imu, uint8_t reg, const uint8_t *data, size_t len);

esp_err_t lsm6dso_spi_read_whoami(const lsm6dso_spi_t *imu, uint8_t *out_whoami);

