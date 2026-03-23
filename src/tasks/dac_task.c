#include "tasks/dac_task.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "control_config.h"
#include "drivers/dac80502_i2c.h"
#include "driver/i2c.h"
#include "modules/dac_control.h"

static const char *TAG = "dac_task";

#ifndef DAC_I2C_ADDR_7BIT_TBD
#define DAC_I2C_ADDR_7BIT_TBD 0x48
#endif

static TaskHandle_t s_dac_task;

static esp_err_t i2c_master_init_dac_bus(void)
{
  const i2c_config_t conf = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = DAC_SDA_GPIO,
      .scl_io_num = DAC_SCL_GPIO,
      .sda_pullup_en = GPIO_PULLUP_DISABLE,
      .scl_pullup_en = GPIO_PULLUP_DISABLE,
      .master.clk_speed = 400000,
  };

  esp_err_t err = i2c_param_config(I2C_NUM_0, &conf);
  if (err != ESP_OK)
    return err;
  err = i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
  if (err == ESP_ERR_INVALID_STATE)
    return ESP_OK;
  return err;
}

static void dac_apply_once(dac80502_i2c_t *dac, uint16_t *last_a, uint16_t *last_b)
{
  const dac_targets_t t = dac_control_get_targets();

  uint16_t a = t.ld_code;
  uint16_t b = t.tec_code;
  if (t.clamp)
  {
    a = DAC_LD_CODE_SAFE_TBD;
    b = DAC_TEC_CODE_SAFE_TBD;
  }

  if (a != *last_a)
  {
    (void)dac80502_i2c_write_reg(dac, DAC80502_REG_DAC_A_DATA, a);
    *last_a = a;
  }
  if (b != *last_b)
  {
    (void)dac80502_i2c_write_reg(dac, DAC80502_REG_DAC_B_DATA, b);
    *last_b = b;
  }
}

static void dac_task(void *arg)
{
  (void)arg;
  // NOTE: s_dac_task is set by xTaskCreatePinnedToCore() so other tasks can
  // notify us immediately after creation. Keep this assignment as a sanity
  // fallback in case the start function changes.
  if (!s_dac_task)
  {
    s_dac_task = xTaskGetCurrentTaskHandle();
  }

  dac_control_init();

  if (i2c_master_init_dac_bus() != ESP_OK)
  {
    ESP_LOGE(TAG, "DAC I2C bus init failed");
  }

  dac80502_i2c_t dac = {0};
  (void)dac80502_i2c_init(&dac, I2C_NUM_0, DAC_I2C_ADDR_7BIT_TBD);

  // Force safe defaults at startup.
  (void)dac80502_i2c_write_reg(&dac, DAC80502_REG_DAC_A_DATA, DAC_LD_CODE_SAFE_TBD);
  (void)dac80502_i2c_write_reg(&dac, DAC80502_REG_DAC_B_DATA, DAC_TEC_CODE_SAFE_TBD);

  uint16_t last_a = DAC_LD_CODE_SAFE_TBD;
  uint16_t last_b = DAC_TEC_CODE_SAFE_TBD;

  for (;;)
  {
    // Block until supervisor/control notifies that targets/clamp changed.
    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    dac_apply_once(&dac, &last_a, &last_b);
  }
}

void dac_task_start(void)
{
  xTaskCreatePinnedToCore(dac_task, "dac", 4096, NULL, 6, &s_dac_task, 0);
  ESP_LOGI(TAG, "DAC task started (event-driven)");
}

void dac_task_notify_update(void)
{
  if (!s_dac_task)
  {
    return;
  }
  xTaskNotifyGive(s_dac_task);
}
