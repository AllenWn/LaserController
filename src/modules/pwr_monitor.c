#include "modules/pwr_monitor.h"

#include <string.h>

#include "driver/gpio.h"
#include "esp_check.h"

static pwr_monitor_config_t s_cfg;
static bool s_inited;

typedef struct
{
  uint8_t tec_cnt;
  uint8_t ld_cnt;
  bool tec_ready;
  bool ld_ready;
} pwr_internal_t;

static pwr_internal_t s_int;

static bool read_active_level(gpio_num_t gpio, bool active_high)
{
  if (gpio == GPIO_NUM_NC)
  {
    return false;
  }
  const bool is_high = gpio_get_level(gpio) != 0;
  return active_high ? is_high : !is_high;
}

static bool update_ready(bool pgood, uint8_t assert_n, uint8_t *cnt, bool *ready)
{
  if (!pgood)
  {
    *cnt = 0;
    *ready = false;
    return *ready;
  }

  if (*ready)
  {
    return true;
  }

  if (*cnt < 255)
  {
    (*cnt)++;
  }
  if (*cnt >= assert_n)
  {
    *ready = true;
  }
  return *ready;
}

esp_err_t pwr_monitor_init(const pwr_monitor_config_t *cfg)
{
  if (!cfg)
  {
    return ESP_ERR_INVALID_ARG;
  }

  s_cfg = *cfg;
  memset(&s_int, 0, sizeof(s_int));

  if (s_cfg.assert_debounce_samples == 0)
  {
    s_cfg.assert_debounce_samples = 1;
  }

  uint64_t in_mask = 0;
  if (s_cfg.pwr_tec_pgood_gpio != GPIO_NUM_NC)
    in_mask |= (1ULL << (int)s_cfg.pwr_tec_pgood_gpio);
  if (s_cfg.pwr_ld_pgood_gpio != GPIO_NUM_NC)
    in_mask |= (1ULL << (int)s_cfg.pwr_ld_pgood_gpio);

  if (in_mask != 0)
  {
    gpio_config_t in_cfg = {
        .pin_bit_mask = in_mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&in_cfg), "pwr_mon", "gpio_config failed");
  }

  s_inited = true;
  return ESP_OK;
}

esp_err_t pwr_monitor_sample(pwr_monitor_status_t *out)
{
  if (!s_inited || !out)
  {
    return ESP_ERR_INVALID_STATE;
  }

  memset(out, 0, sizeof(*out));

  out->tec_pgood = read_active_level(s_cfg.pwr_tec_pgood_gpio, s_cfg.tec_pgood_active_high);
  out->ld_pgood = read_active_level(s_cfg.pwr_ld_pgood_gpio, s_cfg.ld_pgood_active_high);

  out->tec_ready = update_ready(out->tec_pgood, s_cfg.assert_debounce_samples, &s_int.tec_cnt, &s_int.tec_ready);
  out->ld_ready = update_ready(out->ld_pgood, s_cfg.assert_debounce_samples, &s_int.ld_cnt, &s_int.ld_ready);

  return ESP_OK;
}

