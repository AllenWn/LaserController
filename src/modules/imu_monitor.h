#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/spi_master.h"
#include "esp_err.h"

// IMU orientation monitor (LSM6DSO over SPI).
// Goal: allow laser emission only when the laser axis points "down".
//
// Safety rule (as requested):
// - If the laser axis tilt reaches "30° below horizontal", cut emission.
//   Equivalent: angle between laser axis and gravity-down >= 60° => NOT OK.
//
// NOTE: You must define which IMU body axis corresponds to the laser axis.

typedef enum
{
  IMU_AXIS_X_POS = 0,
  IMU_AXIS_X_NEG = 1,
  IMU_AXIS_Y_POS = 2,
  IMU_AXIS_Y_NEG = 3,
  IMU_AXIS_Z_POS = 4,
  IMU_AXIS_Z_NEG = 5,
} imu_axis_t;

typedef enum
{
  IMU_XL_FS_2G = 2,
  IMU_XL_FS_4G = 4,
  IMU_XL_FS_8G = 8,
  IMU_XL_FS_16G = 16,
} imu_xl_fs_t;

typedef struct
{
  // SPI wiring
  spi_host_device_t host;
  int sclk_gpio;
  int mosi_gpio;
  int miso_gpio;
  int cs_gpio;
  int spi_mode; // 0 or 3
  int clock_hz;

  // Which axis is the laser pointing direction in the IMU frame (TBD in mechanical integration).
  imu_axis_t laser_axis;

  // Accelerometer config
  imu_xl_fs_t xl_full_scale_g; // default 2g

  // Validity gate: accelerometer magnitude must be in this range (g) to trust tilt.
  float accel_mag_min_g; // default 0.5
  float accel_mag_max_g; // default 1.5

  // Orientation threshold:
  // - ok when angle_from_down_deg <= max_angle_from_down_deg
  // - fault when angle_from_down_deg >= max_angle_from_down_deg (N samples)
  // - recover when angle_from_down_deg <= (max_angle_from_down_deg - hysteresis_deg) (M samples)
  float max_angle_from_down_deg; // default 60 (30° below horizontal)
  float hysteresis_deg;          // default 5
  uint8_t fault_debounce_samples;
  uint8_t recover_debounce_samples;
} imu_monitor_config_t;

typedef struct
{
  bool valid;
  float ax_g;
  float ay_g;
  float az_g;
  float accel_mag_g;

  float angle_from_down_deg;
  bool orientation_ok;
} imu_monitor_status_t;

esp_err_t imu_monitor_init(const imu_monitor_config_t *cfg);
esp_err_t imu_monitor_sample(imu_monitor_status_t *out);

