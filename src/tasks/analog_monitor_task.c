#include "tasks/analog_monitor_task.h"

#include <math.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "modules/ld_monitor.h"
#include "modules/tec_monitor.h"
#include "system_status.h"

static const char *TAG = "ana_mon";

#ifndef ANALOG_MONITOR_PERIOD_MS
#define ANALOG_MONITOR_PERIOD_MS 100
#endif

static void analog_monitor_task(void *arg)
{
  (void)arg;

  TickType_t last = xTaskGetTickCount();
  for (;;)
  {
    vTaskDelayUntil(&last, pdMS_TO_TICKS(ANALOG_MONITOR_PERIOD_MS));
    const TickType_t now = xTaskGetTickCount();

    ld_monitor_analog_status_t ld = {0};
    if (ld_monitor_sample_analog(&ld) == ESP_OK)
    {
      const ld_analog_status_t st = {
          .ok = ld.ok,
          .temp_valid = ld.tmo_valid,
          .temp_c = ld.temp_c,
          .current_valid = ld.lio_valid,
          .current_a = ld.current_a,
          .updated_at = now,
      };
      system_status_update_ld_analog(&st, now);
    }

    tec_monitor_analog_status_t tec = {0};
    if (tec_monitor_sample_analog(&tec) == ESP_OK)
    {
      const tec_analog_status_t st = {
          .ok = tec.ok,
          .temp_valid = tec.tmo_valid,
          .temp_c = tec.temp_c,
          .itec_valid = tec.itec_valid,
          .itec_a = tec.itec_a,
          .vtec_valid = tec.vtec_valid,
          .vtec_v = tec.vtec_v,
          .updated_at = now,
      };
      system_status_update_tec_analog(&st, now);
    }
  }
}

void analog_monitor_task_start(void)
{
  // Note: LD/TEC monitors are initialized by the digital monitor task in this stage.
  xTaskCreatePinnedToCore(analog_monitor_task, "ana_mon", 4096, NULL, 8, NULL, 0);
  ESP_LOGI(TAG, "Analog monitor task started (period=%dms)", (int)ANALOG_MONITOR_PERIOD_MS);
}

