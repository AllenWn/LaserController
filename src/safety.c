#include "safety.h"

#include "app_config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "hal/gpio_ll.h"
#include "portmacro.h"
#include "soc/gpio_struct.h"

static const char *TAG = "safety";

static EventGroupHandle_t s_safety_bits;
static volatile bool s_laser_cmd;
static volatile uint32_t s_force_off;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

static TaskHandle_t s_safety_task;

static inline void laser_gate_write(bool on)
{
  // Keep this simple and deterministic; laser output is active-high.
  // Use LL to remain ISR-safe when called from notify paths.
  gpio_ll_set_level(&GPIO, (int)LASER_GATE_GPIO, on ? 1 : 0);
}

void safety_init(void)
{
  s_safety_bits = xEventGroupCreate();
  portENTER_CRITICAL(&s_lock);
  s_laser_cmd = false;
  s_force_off = 0;
  portEXIT_CRITICAL(&s_lock);

  gpio_config_t out_cfg = {
      .pin_bit_mask = (1ULL << LASER_GATE_GPIO) | (1ULL << STATUS_LED_GPIO),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_ERROR_CHECK(gpio_config(&out_cfg));

  // Fail-safe default
  laser_gate_write(false);
  gpio_set_level(STATUS_LED_GPIO, 0);

  ESP_LOGI(TAG, "Safety initialized. Laser gate GPIO=%d (OFF)", (int)LASER_GATE_GPIO);
}

void safety_set_condition(safety_bit_t bit, bool ok)
{
  if (!s_safety_bits)
  {
    return;
  }

  if (ok)
  {
    xEventGroupSetBits(s_safety_bits, (EventBits_t)bit);
  }
  else
  {
    xEventGroupClearBits(s_safety_bits, (EventBits_t)bit);
  }
}

void safety_set_laser_command(bool want_on)
{
  portENTER_CRITICAL(&s_lock);
  s_laser_cmd = want_on;
  portEXIT_CRITICAL(&s_lock);
}

bool safety_get_laser_command(void)
{
  portENTER_CRITICAL(&s_lock);
  const bool cmd = s_laser_cmd;
  portEXIT_CRITICAL(&s_lock);
  return cmd;
}

uint32_t safety_get_force_off_reasons(void)
{
  portENTER_CRITICAL(&s_lock);
  const uint32_t reasons = s_force_off;
  portEXIT_CRITICAL(&s_lock);
  return reasons;
}

void safety_force_laser_off(laser_force_off_reason_t reason)
{
  if (reason == LASER_FORCE_NONE)
  {
    return;
  }
  portENTER_CRITICAL(&s_lock);
  s_force_off |= (uint32_t)reason;
  portEXIT_CRITICAL(&s_lock);

  // Immediate hard off.
  laser_gate_write(false);
  if (STATUS_LED_GPIO >= 0)
  {
    gpio_set_level(STATUS_LED_GPIO, 1);
  }
}

void IRAM_ATTR safety_force_laser_off_from_isr(laser_force_off_reason_t reason)
{
  if (reason == LASER_FORCE_NONE)
  {
    return;
  }

  portENTER_CRITICAL_ISR(&s_lock);
  s_force_off |= (uint32_t)reason;
  portEXIT_CRITICAL_ISR(&s_lock);

  laser_gate_write(false);
}

bool safety_get_laser_permit(void)
{
  if (!s_safety_bits)
  {
    return false;
  }

  portENTER_CRITICAL(&s_lock);
  const uint32_t force_off = s_force_off;
  portEXIT_CRITICAL(&s_lock);
  if (force_off != 0)
  {
    return false;
  }

  const EventBits_t bits = xEventGroupGetBits(s_safety_bits);
  const EventBits_t required = (EventBits_t)(SAFETY_BIT_TEC_READY | SAFETY_BIT_TEC_OK | SAFETY_BIT_LD_OK |
                                             SAFETY_BIT_ORIENTATION_OK | SAFETY_BIT_DISTANCE_OK |
                                             SAFETY_BIT_INTERLOCK_OK);
  return (bits & required) == required;
}

void safety_apply_outputs(void)
{
  const bool permit = safety_get_laser_permit();
  portENTER_CRITICAL(&s_lock);
  const bool cmd = s_laser_cmd;
  portEXIT_CRITICAL(&s_lock);
  const bool on = permit && cmd;

  laser_gate_write(on);
  gpio_set_level(STATUS_LED_GPIO, on ? 1 : 0);
}

void safety_notify_from_isr(void)
{
  if (!s_safety_task)
  {
    return;
  }

  BaseType_t woken = pdFALSE;
  vTaskNotifyGiveFromISR(s_safety_task, &woken);
  if (woken)
  {
    portYIELD_FROM_ISR();
  }
}

void safety_bind_task_handle(TaskHandle_t task)
{
  s_safety_task = task;
}
