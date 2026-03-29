#include "tasks/pd_task.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "drivers/stusb4500_i2c.h"
#include "modules/pd_i2c_bus.h"
#include "system_status.h"

static const char *TAG = "pd_task";

#ifndef PD_TASK_PERIOD_MS
#define PD_TASK_PERIOD_MS 200
#endif

#ifndef PD_STATUS_LOG_PERIOD_MS
#define PD_STATUS_LOG_PERIOD_MS 2000
#endif

#ifndef STUSB4500_I2C_ADDR_7BIT
#define STUSB4500_I2C_ADDR_7BIT 0x28
#endif

static void pd_task(void *arg)
{
  (void)arg;

  if (!pd_i2c_bus_config_is_valid())
  {
    ESP_LOGW(TAG, "PD task disabled: PD I2C pins not valid");
    vTaskDelete(NULL);
    return;
  }

  ESP_ERROR_CHECK(pd_i2c_bus_init());

  stusb4500_i2c_t dev = {0};
  ESP_ERROR_CHECK(stusb4500_i2c_init(&dev, pd_i2c_bus_port(), STUSB4500_I2C_ADDR_7BIT));

  TickType_t last_wake = xTaskGetTickCount();
  TickType_t last_log = last_wake;

  for (;;)
  {
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(PD_TASK_PERIOD_MS));

    pd_status_t st;
    memset(&st, 0, sizeof(st));

    if (pd_i2c_bus_lock(pdMS_TO_TICKS(50)))
    {
      const esp_err_t err = stusb4500_i2c_probe(&dev);
      st.i2c_ok = (err == ESP_OK);
      st.device_present = st.i2c_ok;

      // Placeholder policy:
      // - Physical STUSB4500 reachability is already useful to firmware.
      // - Detailed attach / contract / negotiation decoding is intentionally
      //   left TBD until the register map and desired policy are finalized.
      st.attached_known = false;
      st.contract_valid_known = false;
      st.fault_known = false;

      pd_i2c_bus_unlock();
    }
    else
    {
      st.i2c_ok = false;
      st.device_present = false;
    }

    const TickType_t now = xTaskGetTickCount();
    system_status_update_pd(&st, now);

    if ((now - last_log) >= pdMS_TO_TICKS(PD_STATUS_LOG_PERIOD_MS))
    {
      last_log = now;
      ESP_LOGI(TAG, "pd present=%d i2c_ok=%d attach?=%d contract?=%d fault?=%d", st.device_present ? 1 : 0,
               st.i2c_ok ? 1 : 0, st.attached_known ? 1 : 0, st.contract_valid_known ? 1 : 0,
               st.fault_known ? 1 : 0);
    }
  }
}

void pd_task_start(void)
{
  xTaskCreatePinnedToCore(pd_task, "pd_task", 4096, NULL, 5, NULL, 0);
  ESP_LOGI(TAG, "PD task started");
}
