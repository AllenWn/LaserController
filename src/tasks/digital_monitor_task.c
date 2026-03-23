#include "tasks/digital_monitor_task.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "modules/ld_monitor.h"
#include "modules/pwr_monitor.h"
#include "modules/tec_monitor.h"
#include "system_status.h"

static const char *TAG = "dig_mon";

#ifndef DIGITAL_MONITOR_PERIOD_MS
#define DIGITAL_MONITOR_PERIOD_MS 10
#endif

// Digital-good polarity (confirmed): HIGH means GOOD.
#define PWR_TEC_GOOD_ACTIVE_HIGH 1
#define PWR_LD_PGOOD_ACTIVE_HIGH 1
#define LD_LPGD_ACTIVE_HIGH 1
#define TEC_TEMPGD_ACTIVE_HIGH 1

static void digital_monitor_task(void *arg)
{
  (void)arg;

  const pwr_monitor_config_t pwr_cfg = {
      .pwr_tec_pgood_gpio = PWR_TEC_GOOD_GPIO,
      .pwr_ld_pgood_gpio = PWR_LD_PGOOD_GPIO,
      .tec_pgood_active_high = PWR_TEC_GOOD_ACTIVE_HIGH,
      .ld_pgood_active_high = PWR_LD_PGOOD_ACTIVE_HIGH,
      .assert_debounce_samples = 2,
  };

  const ld_monitor_config_t ld_cfg = {
      .pwr_ld_pgood_gpio = PWR_LD_PGOOD_GPIO,
      .ld_lpgd_gpio = LD_LPGD_GPIO,
      .ld_tmo_gpio = LD_TMO_GPIO,
      .ld_lio_gpio = LD_LIO_GPIO,
      .use_tmo_for_safety = false,
      .use_lio_for_safety = false,
      .read_voltage = NULL,
      .pwr_ld_pgood_active_high = PWR_LD_PGOOD_ACTIVE_HIGH,
      .ld_lpgd_active_high = LD_LPGD_ACTIVE_HIGH,
      .tmo_temp_a_c = 0.0f,
      .tmo_temp_b_c_per_v = 0.0f,
      .lio_amps_per_v = 0.0f,
      .temp_max_c = 0.0f,
      .temp_hyst_c = 0.0f,
      .current_max_a = 0.0f,
      .current_hyst_a = 0.0f,
      .fault_debounce_samples = 1,
      .recover_debounce_samples = 3,
  };

  const tec_monitor_config_t tec_cfg = {
      .pwr_tec_pgood_gpio = PWR_TEC_GOOD_GPIO,
      .tec_tempgd_gpio = TEC_TEMPGD_GPIO,
      .tec_tmo_gpio = TEC_TMO_GPIO,
      .tec_itec_gpio = TEC_ITEC_GPIO,
      .tec_vtec_gpio = TEC_VTEC_GPIO,
      .use_tmo_for_safety = false,
      .use_itec_for_safety = false,
      .use_vtec_for_safety = false,
      .read_voltage = NULL,
      .pwr_tec_pgood_active_high = PWR_TEC_GOOD_ACTIVE_HIGH,
      .tec_tempgd_active_high = TEC_TEMPGD_ACTIVE_HIGH,
      .tmo_temp_a_c = 0.0f,
      .tmo_temp_b_c_per_v = 0.0f,
      .itec_v_offset_v = 0.0f,
      .itec_v_per_a = 0.0f,
      .vtec_scale = 0.0f,
      .temp_max_c = 0.0f,
      .temp_hyst_c = 0.0f,
      .itec_max_a = 0.0f,
      .itec_hyst_a = 0.0f,
      .vtec_max_v = 0.0f,
      .vtec_hyst_v = 0.0f,
      .fault_debounce_samples = 1,
      .recover_debounce_samples = 3,
  };

  ESP_ERROR_CHECK(pwr_monitor_init(&pwr_cfg));
  ESP_ERROR_CHECK(ld_monitor_init(&ld_cfg));
  ESP_ERROR_CHECK(tec_monitor_init(&tec_cfg));

  TickType_t last = xTaskGetTickCount();
  for (;;)
  {
    vTaskDelayUntil(&last, pdMS_TO_TICKS(DIGITAL_MONITOR_PERIOD_MS));
    const TickType_t now = xTaskGetTickCount();

    pwr_monitor_status_t pwr = {0};
    if (pwr_monitor_sample(&pwr) == ESP_OK)
    {
      system_status_update_pwr(pwr.tec_ready, pwr.ld_ready, now);
    }

    ld_monitor_digital_status_t ld = {0};
    if (ld_monitor_sample_digital(&ld) == ESP_OK)
    {
      ld_digital_status_t st = {
          .ok = ld.ok,
          .pwr_good = ld.pwr_good,
          .loop_good = ld.loop_good,
          .updated_at = now,
      };
      system_status_update_ld_digital(&st, now);
    }

    tec_monitor_digital_status_t tec = {0};
    if (tec_monitor_sample_digital(&tec) == ESP_OK)
    {
      tec_digital_status_t st = {
          .ok = tec.ok,
          .pwr_good = tec.pwr_good,
          .temp_good = tec.temp_good,
          .updated_at = now,
      };
      system_status_update_tec_digital(&st, now);
    }
  }
}

void digital_monitor_task_start(void)
{
  xTaskCreatePinnedToCore(digital_monitor_task, "dig_mon", 4096, NULL, 10, NULL, 0);
  ESP_LOGI(TAG, "Digital monitor task started (period=%dms)", (int)DIGITAL_MONITOR_PERIOD_MS);
}
