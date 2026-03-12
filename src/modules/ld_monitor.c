#include "modules/ld_monitor.h"

#include <math.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_check.h"

static ld_monitor_config_t s_cfg;
static bool s_inited;

typedef struct
{
  bool temp_ok;
  uint8_t temp_fault_cnt;
  uint8_t temp_recover_cnt;

  bool current_ok;
  uint8_t current_fault_cnt;
  uint8_t current_recover_cnt;
} ld_monitor_internal_t;

static ld_monitor_internal_t s_int;

static bool read_active_level(gpio_num_t gpio, bool active_high)
{
  if (gpio == GPIO_NUM_NC)
  {
    return false;
  }
  const bool is_high = gpio_get_level(gpio) != 0;
  return active_high ? is_high : !is_high;
}

static bool eval_threshold_hyst(bool prev_ok, float value, float max, float hyst, uint8_t fault_n, uint8_t recover_n,
                                uint8_t *fault_cnt, uint8_t *recover_cnt)
{
  // Rule:
  // - Fault when value >= max (N consecutive samples)
  // - Recover when value <= (max - hyst) (M consecutive samples)
  // Anything in between keeps the previous state and resets counters.
  const bool over = value >= max;
  const bool recover = value <= (max - hyst);

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

esp_err_t ld_monitor_init(const ld_monitor_config_t *cfg)
{
  if (!cfg)
  {
    return ESP_ERR_INVALID_ARG;
  }

  s_cfg = *cfg;
  memset(&s_int, 0, sizeof(s_int));

  // Defaults for TBD fields (so module behaves deterministically even if caller leaves them 0).
  if (s_cfg.tmo_temp_a_c == 0.0f && s_cfg.tmo_temp_b_c_per_v == 0.0f)
  {
    // TBD: verify against datasheet + board analog scaling.
    s_cfg.tmo_temp_a_c = 192.5576f;
    s_cfg.tmo_temp_b_c_per_v = -90.1040f;
  }
  if (s_cfg.lio_amps_per_v == 0.0f)
  {
    // TBD: verify against datasheet + board analog scaling.
    s_cfg.lio_amps_per_v = 2.4f;
  }
  if (s_cfg.fault_debounce_samples == 0)
  {
    // TBD: choose based on sample period and required response time.
    s_cfg.fault_debounce_samples = 1;
  }
  if (s_cfg.recover_debounce_samples == 0)
  {
    s_cfg.recover_debounce_samples = 3;
  }
  if (s_cfg.temp_hyst_c < 0.0f)
  {
    s_cfg.temp_hyst_c = 0.0f;
  }
  if (s_cfg.current_hyst_a < 0.0f)
  {
    s_cfg.current_hyst_a = 0.0f;
  }

  // Initial OK states:
  // - If analog channels are not used for safety, default to OK.
  // - If they are used, default to NOT OK until we get a valid sample.
  s_int.temp_ok = s_cfg.use_tmo_for_safety ? false : true;
  s_int.current_ok = s_cfg.use_lio_for_safety ? false : true;

  uint64_t in_mask = 0;
  if (s_cfg.pwr_ld_pgood_gpio != GPIO_NUM_NC)
    in_mask |= (1ULL << (int)s_cfg.pwr_ld_pgood_gpio);
  if (s_cfg.ld_lpgd_gpio != GPIO_NUM_NC)
    in_mask |= (1ULL << (int)s_cfg.ld_lpgd_gpio);

  // Analog monitor pins are not configured here yet (ADC path TBD). Keep them untouched.

  if (in_mask != 0)
  {
    gpio_config_t in_cfg = {
        .pin_bit_mask = in_mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&in_cfg), "ld_mon", "gpio_config failed");
  }

  s_inited = true;
  return ESP_OK;
}

static void compute_analog(ld_monitor_analog_status_t *out)
{
  // ADC integration is board-specific. If no read hook is provided, keep invalid.
  out->tmo_valid = false;
  out->tmo_v = NAN;
  out->temp_c = NAN;
  out->temp_ok = s_int.temp_ok;

  out->lio_valid = false;
  out->lio_v = NAN;
  out->current_a = NAN;
  out->current_ok = s_int.current_ok;

  out->ok = true;
  if (s_cfg.use_tmo_for_safety)
  {
    out->ok = out->ok && out->temp_ok;
  }
  if (s_cfg.use_lio_for_safety)
  {
    out->ok = out->ok && out->current_ok;
  }

  if (!s_cfg.read_voltage)
  {
    return;
  }

  // LD_TMO (temperature monitor)
  if (s_cfg.ld_tmo_gpio != GPIO_NUM_NC)
  {
    float v = NAN;
    if (s_cfg.read_voltage(s_cfg.ld_tmo_gpio, &v))
    {
      out->tmo_valid = true;
      out->tmo_v = v;

      // TBD: if PCB scales the voltage, apply scaling here (e.g., v = v * scale_factor).
      out->temp_c = s_cfg.tmo_temp_a_c + (s_cfg.tmo_temp_b_c_per_v * v);

      if (s_cfg.use_tmo_for_safety)
      {
        // TBD: define temp_max_c based on system safety requirements.
        s_int.temp_ok = eval_threshold_hyst(s_int.temp_ok, out->temp_c, s_cfg.temp_max_c, s_cfg.temp_hyst_c,
                                            s_cfg.fault_debounce_samples, s_cfg.recover_debounce_samples,
                                            &s_int.temp_fault_cnt, &s_int.temp_recover_cnt);
      }
      out->temp_ok = s_int.temp_ok;
      if (s_cfg.use_tmo_for_safety)
      {
        out->ok = out->ok && out->temp_ok;
      }
    }
  }

  // LD_LIO (current monitor)
  if (s_cfg.ld_lio_gpio != GPIO_NUM_NC)
  {
    float v = NAN;
    if (s_cfg.read_voltage(s_cfg.ld_lio_gpio, &v))
    {
      out->lio_valid = true;
      out->lio_v = v;

      // TBD: if PCB scales the voltage, apply scaling here.
      out->current_a = v * s_cfg.lio_amps_per_v;

      if (s_cfg.use_lio_for_safety)
      {
        // TBD: define current_max_a based on system safety requirements.
        s_int.current_ok = eval_threshold_hyst(s_int.current_ok, out->current_a, s_cfg.current_max_a, s_cfg.current_hyst_a,
                                               s_cfg.fault_debounce_samples, s_cfg.recover_debounce_samples,
                                               &s_int.current_fault_cnt, &s_int.current_recover_cnt);
      }
      out->current_ok = s_int.current_ok;
      if (s_cfg.use_lio_for_safety)
      {
        out->ok = out->ok && out->current_ok;
      }
    }
  }
}

esp_err_t ld_monitor_sample_digital(ld_monitor_digital_status_t *out)
{
  if (!s_inited || !out)
  {
    return ESP_ERR_INVALID_STATE;
  }

  memset(out, 0, sizeof(*out));

  out->pwr_good = read_active_level(s_cfg.pwr_ld_pgood_gpio, s_cfg.pwr_ld_pgood_active_high);
  out->loop_good = read_active_level(s_cfg.ld_lpgd_gpio, s_cfg.ld_lpgd_active_high);

  // Minimal "OK" definition for LD domain: power good AND loop good.
  out->ok = out->pwr_good && out->loop_good;

  return ESP_OK;
}

esp_err_t ld_monitor_sample_analog(ld_monitor_analog_status_t *out)
{
  if (!s_inited || !out)
  {
    return ESP_ERR_INVALID_STATE;
  }

  memset(out, 0, sizeof(*out));
  compute_analog(out);
  return ESP_OK;
}

esp_err_t ld_monitor_sample(ld_monitor_digital_status_t *dig, ld_monitor_analog_status_t *ana, bool *out_ok)
{
  if (!s_inited || !out_ok)
  {
    return ESP_ERR_INVALID_STATE;
  }

  ld_monitor_digital_status_t d = {0};
  ld_monitor_analog_status_t a = {0};

  ESP_RETURN_ON_ERROR(ld_monitor_sample_digital(&d), "ld_mon", "digital sample failed");
  ESP_RETURN_ON_ERROR(ld_monitor_sample_analog(&a), "ld_mon", "analog sample failed");

  if (dig)
  {
    *dig = d;
  }
  if (ana)
  {
    *ana = a;
  }

  bool ok = d.ok;
  if (s_cfg.use_tmo_for_safety || s_cfg.use_lio_for_safety)
  {
    ok = ok && a.ok;
  }
  *out_ok = ok;
  return ESP_OK;
}
