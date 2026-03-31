#include "modules/tec_monitor.h"

#include <math.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_check.h"

static tec_monitor_config_t s_cfg;
static bool s_inited;

typedef struct
{
  bool temp_ok;
  uint8_t temp_fault_cnt;
  uint8_t temp_recover_cnt;

  bool itec_ok;
  uint8_t itec_fault_cnt;
  uint8_t itec_recover_cnt;

  bool vtec_ok;
  uint8_t vtec_fault_cnt;
  uint8_t vtec_recover_cnt;
} tec_monitor_internal_t;

static tec_monitor_internal_t s_int;

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

esp_err_t tec_monitor_init(const tec_monitor_config_t *cfg)
{
  if (!cfg)
  {
    return ESP_ERR_INVALID_ARG;
  }

  s_cfg = *cfg;
  memset(&s_int, 0, sizeof(s_int));

  // Defaults for known datasheet values and safe deterministic behavior.
  if (s_cfg.fault_debounce_samples == 0)
  {
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
  if (s_cfg.itec_hyst_a < 0.0f)
  {
    s_cfg.itec_hyst_a = 0.0f;
  }
  if (s_cfg.vtec_hyst_v < 0.0f)
  {
    s_cfg.vtec_hyst_v = 0.0f;
  }

  if (s_cfg.itec_v_offset_v == 0.0f && s_cfg.itec_v_per_a == 0.0f)
  {
    s_cfg.itec_v_offset_v = 1.25f;
    s_cfg.itec_v_per_a = 0.285f;
  }

  // Initial OK states.
  s_int.temp_ok = s_cfg.use_tmo_for_safety ? false : true;
  s_int.itec_ok = s_cfg.use_itec_for_safety ? false : true;
  s_int.vtec_ok = s_cfg.use_vtec_for_safety ? false : true;

  uint64_t in_mask = 0;
  if (s_cfg.pwr_tec_pgood_gpio != GPIO_NUM_NC)
    in_mask |= (1ULL << (int)s_cfg.pwr_tec_pgood_gpio);
  if (s_cfg.tec_tempgd_gpio != GPIO_NUM_NC)
    in_mask |= (1ULL << (int)s_cfg.tec_tempgd_gpio);

  if (in_mask != 0)
  {
    gpio_config_t in_cfg = {
        .pin_bit_mask = in_mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&in_cfg), "tec_mon", "gpio_config failed");
  }

  s_inited = true;
  return ESP_OK;
}

static void compute_analog(tec_monitor_analog_status_t *out)
{
  out->tmo_valid = false;
  out->tmo_v = NAN;
  out->temp_c = NAN;
  out->temp_ok = s_int.temp_ok;

  out->itec_valid = false;
  out->itec_v = NAN;
  out->itec_a = NAN;
  out->itec_ok = s_int.itec_ok;

  out->vtec_valid = false;
  out->vtec_v_pin = NAN;
  out->vtec_v = NAN;
  out->vtec_ok = s_int.vtec_ok;

  out->ok = true;
  if (s_cfg.use_tmo_for_safety)
    out->ok = out->ok && out->temp_ok;
  if (s_cfg.use_itec_for_safety)
    out->ok = out->ok && out->itec_ok;
  if (s_cfg.use_vtec_for_safety)
    out->ok = out->ok && out->vtec_ok;

  if (!s_cfg.read_voltage)
  {
    return;
  }

  // TEC_TMO (temperature monitor) -> temperature
  if (s_cfg.tec_tmo_gpio != GPIO_NUM_NC)
  {
    float v = NAN;
    if (s_cfg.read_voltage(s_cfg.tec_tmo_gpio, &v))
    {
      out->tmo_valid = true;
      out->tmo_v = v;

      // Apply board scaling / calibration here once the TMO->temperature mapping
      // is finalized for this board revision.
      out->temp_c = s_cfg.tmo_temp_a_c + (s_cfg.tmo_temp_b_c_per_v * v);

      if (s_cfg.use_tmo_for_safety)
      {
        s_int.temp_ok = eval_threshold_hyst(s_int.temp_ok, out->temp_c, s_cfg.temp_max_c, s_cfg.temp_hyst_c,
                                            s_cfg.fault_debounce_samples, s_cfg.recover_debounce_samples,
                                            &s_int.temp_fault_cnt, &s_int.temp_recover_cnt);
      }
      out->temp_ok = s_int.temp_ok;
      if (s_cfg.use_tmo_for_safety)
        out->ok = out->ok && out->temp_ok;
    }
  }

  // TEC_ITEC (current monitor) -> current
  if (s_cfg.tec_itec_gpio != GPIO_NUM_NC)
  {
    float v = NAN;
    if (s_cfg.read_voltage(s_cfg.tec_itec_gpio, &v))
    {
      out->itec_valid = true;
      out->itec_v = v;

      // Datasheet: ITEC = (V_ITEC - 1.25) / 0.285
      // Apply board scaling here if the PCB changes the monitor voltage before ADC.
      out->itec_a = (v - s_cfg.itec_v_offset_v) / s_cfg.itec_v_per_a;

      if (s_cfg.use_itec_for_safety)
      {
        s_int.itec_ok = eval_threshold_hyst(s_int.itec_ok, out->itec_a, s_cfg.itec_max_a, s_cfg.itec_hyst_a,
                                            s_cfg.fault_debounce_samples, s_cfg.recover_debounce_samples,
                                            &s_int.itec_fault_cnt, &s_int.itec_recover_cnt);
      }
      out->itec_ok = s_int.itec_ok;
      if (s_cfg.use_itec_for_safety)
        out->ok = out->ok && out->itec_ok;
    }
  }

  // TEC_VTEC (voltage monitor) -> TEC voltage
  if (s_cfg.tec_vtec_gpio != GPIO_NUM_NC)
  {
    float v = NAN;
    if (s_cfg.read_voltage(s_cfg.tec_vtec_gpio, &v))
    {
      out->vtec_valid = true;
      out->vtec_v_pin = v;

      // Apply board scaling here once the VTEC path is finalized for this board revision.
      out->vtec_v = v * s_cfg.vtec_scale;

      if (s_cfg.use_vtec_for_safety)
      {
        s_int.vtec_ok = eval_threshold_hyst(s_int.vtec_ok, out->vtec_v, s_cfg.vtec_max_v, s_cfg.vtec_hyst_v,
                                            s_cfg.fault_debounce_samples, s_cfg.recover_debounce_samples,
                                            &s_int.vtec_fault_cnt, &s_int.vtec_recover_cnt);
      }
      out->vtec_ok = s_int.vtec_ok;
      if (s_cfg.use_vtec_for_safety)
        out->ok = out->ok && out->vtec_ok;
    }
  }
}

esp_err_t tec_monitor_sample_digital(tec_monitor_digital_status_t *out)
{
  if (!s_inited || !out)
  {
    return ESP_ERR_INVALID_STATE;
  }

  memset(out, 0, sizeof(*out));
  out->pwr_good = read_active_level(s_cfg.pwr_tec_pgood_gpio, s_cfg.pwr_tec_pgood_active_high);
  out->temp_good = read_active_level(s_cfg.tec_tempgd_gpio, s_cfg.tec_tempgd_active_high);
  out->ok = out->pwr_good && out->temp_good;
  return ESP_OK;
}

esp_err_t tec_monitor_sample_analog(tec_monitor_analog_status_t *out)
{
  if (!s_inited || !out)
  {
    return ESP_ERR_INVALID_STATE;
  }

  memset(out, 0, sizeof(*out));
  compute_analog(out);
  return ESP_OK;
}

esp_err_t tec_monitor_sample(tec_monitor_digital_status_t *dig, tec_monitor_analog_status_t *ana, bool *out_ok)
{
  if (!s_inited || !out_ok)
  {
    return ESP_ERR_INVALID_STATE;
  }

  tec_monitor_digital_status_t d = {0};
  tec_monitor_analog_status_t a = {0};

  ESP_RETURN_ON_ERROR(tec_monitor_sample_digital(&d), "tec_mon", "digital sample failed");
  ESP_RETURN_ON_ERROR(tec_monitor_sample_analog(&a), "tec_mon", "analog sample failed");

  if (dig)
    *dig = d;
  if (ana)
    *ana = a;

  bool ok = d.ok;
  if (s_cfg.use_tmo_for_safety || s_cfg.use_itec_for_safety || s_cfg.use_vtec_for_safety)
  {
    ok = ok && a.ok;
  }
  *out_ok = ok;
  return ESP_OK;
}
