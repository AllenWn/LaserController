#include "modules/imu_monitor.h"

#include <math.h>
#include <string.h>

#include "drivers/lsm6dso_spi.h"
#include "esp_check.h"

static imu_monitor_config_t s_cfg;
static bool s_inited;
static lsm6dso_spi_t s_spi;

typedef struct
{
  bool ok;
  uint8_t fault_cnt;
  uint8_t recover_cnt;
} imu_internal_t;

static imu_internal_t s_int;

static float clamp_f(float v, float lo, float hi)
{
  if (v < lo)
    return lo;
  if (v > hi)
    return hi;
  return v;
}

static void axis_unit_vec(imu_axis_t axis, float *x, float *y, float *z)
{
  *x = 0.0f;
  *y = 0.0f;
  *z = 0.0f;
  switch (axis)
  {
  case IMU_AXIS_X_POS:
    *x = 1.0f;
    break;
  case IMU_AXIS_X_NEG:
    *x = -1.0f;
    break;
  case IMU_AXIS_Y_POS:
    *y = 1.0f;
    break;
  case IMU_AXIS_Y_NEG:
    *y = -1.0f;
    break;
  case IMU_AXIS_Z_POS:
    *z = 1.0f;
    break;
  case IMU_AXIS_Z_NEG:
    *z = -1.0f;
    break;
  default:
    *x = 1.0f;
    break;
  }
}

static float xl_sens_g_per_lsb(imu_xl_fs_t fs_g)
{
  // LSM6DSO typical sensitivities (mg/LSB): 2g=0.061, 4g=0.122, 8g=0.244, 16g=0.488
  switch (fs_g)
  {
  case IMU_XL_FS_2G:
    return 0.000061f;
  case IMU_XL_FS_4G:
    return 0.000122f;
  case IMU_XL_FS_8G:
    return 0.000244f;
  case IMU_XL_FS_16G:
    return 0.000488f;
  default:
    return 0.000061f;
  }
}

static uint8_t ctrl1_xl_from_fs(imu_xl_fs_t fs_g)
{
  // CTRL1_XL (0x10): [7:4] ODR_XL, [3:2] FS_XL, [1:0] BW
  // We'll use ODR=104Hz (0100b) as bring-up default, BW=00.
  const uint8_t odr_104hz = 0x40;
  uint8_t fs_bits = 0;
  switch (fs_g)
  {
  case IMU_XL_FS_2G:
    fs_bits = 0x00;
    break;
  case IMU_XL_FS_16G:
    fs_bits = 0x04;
    break;
  case IMU_XL_FS_4G:
    fs_bits = 0x08;
    break;
  case IMU_XL_FS_8G:
    fs_bits = 0x0C;
    break;
  default:
    fs_bits = 0x00;
    break;
  }
  return (uint8_t)(odr_104hz | fs_bits);
}

static bool eval_angle_hyst(bool prev_ok, float angle_deg, float max_deg, float hyst_deg, uint8_t fault_n,
                            uint8_t recover_n, uint8_t *fault_cnt, uint8_t *recover_cnt)
{
  const float recover_deg = max_deg - hyst_deg;
  const bool over = angle_deg >= max_deg;
  const bool recover = angle_deg <= recover_deg;

  if (prev_ok)
  {
    if (over)
    {
      if (*fault_cnt < 255)
        (*fault_cnt)++;
      *recover_cnt = 0;
      if (*fault_cnt >= fault_n)
      {
        return false;
      }
    }
    else
    {
      *fault_cnt = 0;
      *recover_cnt = 0;
    }
    return true;
  }
  else
  {
    if (recover)
    {
      if (*recover_cnt < 255)
        (*recover_cnt)++;
      *fault_cnt = 0;
      if (*recover_cnt >= recover_n)
      {
        return true;
      }
    }
    else
    {
      *fault_cnt = 0;
      *recover_cnt = 0;
    }
    return false;
  }
}

esp_err_t imu_monitor_init(const imu_monitor_config_t *cfg)
{
  if (!cfg)
  {
    return ESP_ERR_INVALID_ARG;
  }

  s_cfg = *cfg;
  memset(&s_int, 0, sizeof(s_int));

  // Defaults if caller leaves them 0.
  if (s_cfg.xl_full_scale_g == 0)
    s_cfg.xl_full_scale_g = IMU_XL_FS_2G;
  if (s_cfg.accel_mag_min_g == 0.0f && s_cfg.accel_mag_max_g == 0.0f)
  {
    s_cfg.accel_mag_min_g = 0.5f;
    s_cfg.accel_mag_max_g = 1.5f;
  }
  if (s_cfg.max_angle_from_down_deg == 0.0f)
  {
    // Requested: cut at horizontal-down 30deg => angle_from_down=60deg.
    s_cfg.max_angle_from_down_deg = 60.0f;
  }
  if (s_cfg.hysteresis_deg == 0.0f)
  {
    s_cfg.hysteresis_deg = 5.0f;
  }
  if (s_cfg.fault_debounce_samples == 0)
  {
    s_cfg.fault_debounce_samples = 1;
  }
  if (s_cfg.recover_debounce_samples == 0)
  {
    s_cfg.recover_debounce_samples = 3;
  }

  // Default to NOT OK until we have a valid sample.
  s_int.ok = false;

  const lsm6dso_spi_config_t spicfg = {
      .host = s_cfg.host,
      .sclk_gpio = s_cfg.sclk_gpio,
      .mosi_gpio = s_cfg.mosi_gpio,
      .miso_gpio = s_cfg.miso_gpio,
      .cs_gpio = s_cfg.cs_gpio,
      .spi_mode = s_cfg.spi_mode,
      .clock_hz = s_cfg.clock_hz,
  };
  ESP_RETURN_ON_ERROR(lsm6dso_spi_init(&s_spi, &spicfg), "imu_mon", "spi init failed");

  uint8_t who = 0;
  ESP_RETURN_ON_ERROR(lsm6dso_spi_read_whoami(&s_spi, &who), "imu_mon", "WHO_AM_I read failed");
  if (who != 0x6C)
  {
    return ESP_ERR_INVALID_RESPONSE;
  }

  // Minimal configuration:
  // - CTRL3_C (0x12): BDU=1, IF_INC=1
  // - CTRL1_XL (0x10): ODR=104Hz, FS=cfg
  const uint8_t ctrl3_c = 0x44;
  const uint8_t ctrl1_xl = ctrl1_xl_from_fs(s_cfg.xl_full_scale_g);
  ESP_RETURN_ON_ERROR(lsm6dso_spi_write_reg(&s_spi, 0x12, &ctrl3_c, 1), "imu_mon", "CTRL3_C write failed");
  ESP_RETURN_ON_ERROR(lsm6dso_spi_write_reg(&s_spi, 0x10, &ctrl1_xl, 1), "imu_mon", "CTRL1_XL write failed");

  s_inited = true;
  return ESP_OK;
}

static bool read_accel_g(float *ax_g, float *ay_g, float *az_g)
{
  uint8_t buf[6] = {0};
  if (lsm6dso_spi_read_reg(&s_spi, 0x28, buf, sizeof(buf)) != ESP_OK)
  {
    return false;
  }

  const int16_t ax = (int16_t)((buf[1] << 8) | buf[0]);
  const int16_t ay = (int16_t)((buf[3] << 8) | buf[2]);
  const int16_t az = (int16_t)((buf[5] << 8) | buf[4]);

  const float k = xl_sens_g_per_lsb(s_cfg.xl_full_scale_g);
  *ax_g = (float)ax * k;
  *ay_g = (float)ay * k;
  *az_g = (float)az * k;
  return true;
}

esp_err_t imu_monitor_sample(imu_monitor_status_t *out)
{
  if (!s_inited || !out)
  {
    return ESP_ERR_INVALID_STATE;
  }

  memset(out, 0, sizeof(*out));
  out->valid = false;
  out->orientation_ok = s_int.ok;
  out->angle_from_down_deg = NAN;

  float ax = 0, ay = 0, az = 0;
  if (!read_accel_g(&ax, &ay, &az))
  {
    // Fail-safe: cannot sample => invalid.
    s_int.ok = false;
    out->orientation_ok = false;
    return ESP_OK;
  }

  out->ax_g = ax;
  out->ay_g = ay;
  out->az_g = az;

  const float mag = sqrtf(ax * ax + ay * ay + az * az);
  out->accel_mag_g = mag;
  if (!(mag >= s_cfg.accel_mag_min_g && mag <= s_cfg.accel_mag_max_g))
  {
    // Not a stable gravity vector; treat as invalid / not OK.
    s_int.ok = false;
    out->orientation_ok = false;
    return ESP_OK;
  }

  out->valid = true;

  // Gravity-down unit vector in body frame:
  // accelerometer measures "up" (depends on convention), but for a static device this works well:
  // g_down = -a_unit
  const float inv = 1.0f / mag;
  const float gx = -ax * inv;
  const float gy = -ay * inv;
  const float gz = -az * inv;

  // Laser axis unit vector in body frame (TBD: confirm with mechanical integration).
  float lx = 0, ly = 0, lz = 0;
  axis_unit_vec(s_cfg.laser_axis, &lx, &ly, &lz);

  // Angle between laser axis and gravity-down.
  const float dot = clamp_f(lx * gx + ly * gy + lz * gz, -1.0f, 1.0f);
  const float angle = acosf(dot) * (180.0f / (float)M_PI);
  out->angle_from_down_deg = angle;

  s_int.ok = eval_angle_hyst(s_int.ok, angle, s_cfg.max_angle_from_down_deg, s_cfg.hysteresis_deg,
                             s_cfg.fault_debounce_samples, s_cfg.recover_debounce_samples, &s_int.fault_cnt,
                             &s_int.recover_cnt);
  out->orientation_ok = s_int.ok;

  return ESP_OK;
}

