#include "tasks/trigger_input_task.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "driver/gpio.h"
#include "tasks/supervisor_task.h"

static const char *TAG = "trig_in";

#ifndef TRIGGER_INPUT_PERIOD_MS
#define TRIGGER_INPUT_PERIOD_MS 5
#endif

#ifndef TRIGGER_ACTIVE_HIGH_TBD
#define TRIGGER_ACTIVE_HIGH_TBD 1
#endif

#ifndef TRIGGER_DEBOUNCE_SAMPLES
#define TRIGGER_DEBOUNCE_SAMPLES 3
#endif

static bool read_trigger_raw(void)
{
  const bool level_high = gpio_get_level(TRIGGER_BUTTON_GPIO) != 0;
  return TRIGGER_ACTIVE_HIGH_TBD ? level_high : !level_high;
}

static void trigger_input_task(void *arg)
{
  (void)arg;

  gpio_config_t in_cfg = {
      .pin_bit_mask = (1ULL << (int)TRIGGER_BUTTON_GPIO),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_ERROR_CHECK(gpio_config(&in_cfg));

  bool stable = false;
  uint8_t same_cnt = 0;
  bool last_raw = read_trigger_raw();
  supervisor_set_trigger(false);

  TickType_t last = xTaskGetTickCount();
  for (;;)
  {
    vTaskDelayUntil(&last, pdMS_TO_TICKS(TRIGGER_INPUT_PERIOD_MS));

    const bool raw = read_trigger_raw();
    if (raw == last_raw)
    {
      if (same_cnt < 255)
      {
        same_cnt++;
      }
    }
    else
    {
      same_cnt = 0;
      last_raw = raw;
    }

    if (same_cnt >= TRIGGER_DEBOUNCE_SAMPLES && stable != raw)
    {
      stable = raw;
      supervisor_set_trigger(stable);
      ESP_LOGI(TAG, "Trigger=%d", stable ? 1 : 0);
    }
  }
}

void trigger_input_task_start(void)
{
  xTaskCreatePinnedToCore(trigger_input_task, "trig_in", 2048, NULL, 11, NULL, 0);
  ESP_LOGI(TAG, "Trigger input task started (gpio=%d period=%dms)", (int)TRIGGER_BUTTON_GPIO,
           (int)TRIGGER_INPUT_PERIOD_MS);
}
