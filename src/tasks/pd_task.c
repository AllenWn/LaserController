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

// These register/bit values are treated as provisional until the exact
// STUSB4500 register map is confirmed against the final datasheet revision
// used by this project.
#ifndef STUSB4500_REG_ALERT_STATUS_1_TBD
#define STUSB4500_REG_ALERT_STATUS_1_TBD 0x0B
#endif

#ifndef STUSB4500_REG_PORT_STATUS_0_TBD
#define STUSB4500_REG_PORT_STATUS_0_TBD 0x0D
#endif

#ifndef STUSB4500_REG_PORT_STATUS_1_TBD
#define STUSB4500_REG_PORT_STATUS_1_TBD 0x0E
#endif

#ifndef STUSB4500_ALERT_FAULT_MASK_TBD
#define STUSB4500_ALERT_FAULT_MASK_TBD 0x1Fu
#endif

#ifndef STUSB4500_PORT_STATUS_ATTACHED_MASK_TBD
#define STUSB4500_PORT_STATUS_ATTACHED_MASK_TBD 0x01u
#endif

#ifndef STUSB4500_PORT_STATUS_CONTRACT_VALID_MASK_TBD
#define STUSB4500_PORT_STATUS_CONTRACT_VALID_MASK_TBD 0x08u
#endif

static bool pd_status_equal(const pd_status_t *a, const pd_status_t *b)
{
  return a->device_present == b->device_present && a->i2c_ok == b->i2c_ok && a->attached_known == b->attached_known &&
         a->attached == b->attached && a->contract_valid_known == b->contract_valid_known &&
         a->contract_valid == b->contract_valid && a->fault_known == b->fault_known && a->fault == b->fault;
}

static bool read_reg_locked(const stusb4500_i2c_t *dev, uint8_t reg, uint8_t *value)
{
  return stusb4500_i2c_read_reg8(dev, reg, value) == ESP_OK;
}

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
  pd_status_t last_status = {0};
  bool have_last_status = false;

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

      if (st.i2c_ok)
      {
        uint8_t alert_status_1 = 0;
        uint8_t port_status_0 = 0;
        uint8_t port_status_1 = 0;

        const bool have_alert = read_reg_locked(&dev, STUSB4500_REG_ALERT_STATUS_1_TBD, &alert_status_1);
        const bool have_port0 = read_reg_locked(&dev, STUSB4500_REG_PORT_STATUS_0_TBD, &port_status_0);
        const bool have_port1 = read_reg_locked(&dev, STUSB4500_REG_PORT_STATUS_1_TBD, &port_status_1);

        st.fault_known = have_alert;
        if (have_alert)
        {
          st.fault = (alert_status_1 & STUSB4500_ALERT_FAULT_MASK_TBD) != 0u;
        }

        st.attached_known = have_port0;
        if (have_port0)
        {
          st.attached = (port_status_0 & STUSB4500_PORT_STATUS_ATTACHED_MASK_TBD) != 0u;
        }

        st.contract_valid_known = have_port1;
        if (have_port1)
        {
          st.contract_valid = (port_status_1 & STUSB4500_PORT_STATUS_CONTRACT_VALID_MASK_TBD) != 0u;
        }
      }

      pd_i2c_bus_unlock();
    }
    else
    {
      st.i2c_ok = false;
      st.device_present = false;
    }

    const TickType_t now = xTaskGetTickCount();
    system_status_update_pd(&st, now);

    if (!have_last_status || !pd_status_equal(&st, &last_status))
    {
      ESP_LOGI(TAG,
               "pd present=%d i2c_ok=%d attach=%d/%d contract=%d/%d fault=%d/%d",
               st.device_present ? 1 : 0, st.i2c_ok ? 1 : 0, st.attached_known ? 1 : 0, st.attached ? 1 : 0,
               st.contract_valid_known ? 1 : 0, st.contract_valid ? 1 : 0, st.fault_known ? 1 : 0,
               st.fault ? 1 : 0);
      last_status = st;
      have_last_status = true;
    }
  }
}

void pd_task_start(void)
{
  xTaskCreatePinnedToCore(pd_task, "pd_task", 4096, NULL, 5, NULL, 0);
  ESP_LOGI(TAG, "PD task started");
}
