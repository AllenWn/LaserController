#include "board_io.h"

#include "app_config.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "board_io";

static void gpio_write(gpio_num_t gpio, bool high)
{
  gpio_set_level(gpio, high ? 1 : 0);
}

static void set_pwr_tec_en(bool en) { gpio_write(PWR_TEC_EN_GPIO, en); }
static void set_pwr_ld_en(bool en) { gpio_write(PWR_LD_EN_GPIO, en); }

static void set_ld_pcn_low(void) { gpio_write(LD_PCN_GPIO, false); }

static void set_ld_sbdn_shutdown(void) { gpio_write(LD_SBDN_GPIO, false); }
static void set_ld_sbdn_operate(void) { gpio_write(LD_SBDN_GPIO, true); }

static void apply_ld_mode(ld_mode_t mode)
{
  switch (mode)
  {
  case LD_MODE_SHUTDOWN:
    set_ld_sbdn_shutdown();
    break;
  case LD_MODE_OPERATE:
    set_ld_sbdn_operate();
    break;
  case LD_MODE_STANDBY:
  default:
    // TBD: ATLS6A214D SBDN supports a mid-level "standby", but ESP32 GPIO is 0/3.3V.
    // Unless the PCB provides an analog level network, treat STANDBY as SHUTDOWN (fail-safe).
    set_ld_sbdn_shutdown();
    break;
  }
}

esp_err_t board_io_init(void)
{
  // Safety-critical outputs used by current design.
  const uint64_t out_mask =
      (1ULL << (int)PWR_TEC_EN_GPIO) | (1ULL << (int)PWR_LD_EN_GPIO) | (1ULL << (int)LD_SBDN_GPIO) |
      (1ULL << (int)LD_PCN_GPIO);

  gpio_config_t out_cfg = {
      .pin_bit_mask = out_mask,
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_RETURN_ON_ERROR(gpio_config(&out_cfg), TAG, "gpio_config outputs failed");

  // Fail-safe defaults.
  set_pwr_tec_en(false);
  set_pwr_ld_en(false);
  set_ld_sbdn_shutdown();
  set_ld_pcn_low();

  ESP_LOGI(TAG, "IO initialized (fail-safe outputs set)");
  return ESP_OK;
}

esp_err_t board_io_apply_fsm_outputs(const fsm_outputs_t *out, bool permit, bool fault_present)
{
  if (!out)
  {
    return ESP_ERR_INVALID_ARG;
  }

  // Fault override: force everything safe.
  if (fault_present)
  {
    set_pwr_tec_en(false);
    set_pwr_ld_en(false);
    set_ld_sbdn_shutdown();
    set_ld_pcn_low();
    return ESP_OK;
  }

  // Apply power enables from FSM.
  set_pwr_tec_en(out->enable_tec_power);
  set_pwr_ld_en(out->enable_ld_power);

  // PCN policy is not defined yet; keep a safe deterministic default.
  // TBD: implement real power-level selection strategy.
  set_ld_pcn_low();

  // Emission gating:
  // - permit false => force LD shutdown
  // - want_emission false => also keep LD in safe mode (even if READY selects operate)
  if (!permit || !out->want_emission)
  {
    set_ld_sbdn_shutdown();
    return ESP_OK;
  }

  // FIRING path: allow LD operate.
  apply_ld_mode(out->ld_mode);
  return ESP_OK;
}

