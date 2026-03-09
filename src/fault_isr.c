#include "fault_isr.h"

#include "app_config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_intr_alloc.h"
#include "safety.h"

static const char *TAG = "fault_isr";

static void IRAM_ATTR fault_gpio_isr(void *arg)
{
  const int gpio = (int)(intptr_t)arg;

  if (gpio == (int)LD_FAULT_N_GPIO)
  {
    safety_force_laser_off_from_isr(LASER_FORCE_LD_FAULT);
  }
  else if (gpio == (int)TEC_FAULT_N_GPIO)
  {
    safety_force_laser_off_from_isr(LASER_FORCE_TEC_FAULT);
  }
  else if (gpio == (int)ESTOP_N_GPIO)
  {
    safety_force_laser_off_from_isr(LASER_FORCE_ESTOP);
  }
  else
  {
    safety_force_laser_off_from_isr(LASER_FORCE_ESTOP);
  }

  safety_notify_from_isr();
}

static void configure_fault_input(gpio_num_t gpio)
{
  gpio_config_t cfg = {
      .pin_bit_mask = (1ULL << gpio),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_NEGEDGE, // active-low fault asserted
  };
  ESP_ERROR_CHECK(gpio_config(&cfg));

  ESP_ERROR_CHECK(gpio_isr_handler_add(gpio, fault_gpio_isr, (void *)(intptr_t)gpio));
}

void fault_isr_init(void)
{
  ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM));

  configure_fault_input(LD_FAULT_N_GPIO);
  configure_fault_input(TEC_FAULT_N_GPIO);
  configure_fault_input(ESTOP_N_GPIO);

  ESP_LOGI(TAG, "Fault ISRs installed (LD=%d TEC=%d ESTOP=%d)", (int)LD_FAULT_N_GPIO, (int)TEC_FAULT_N_GPIO,
           (int)ESTOP_N_GPIO);
}
