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

#ifndef DAC_TASK_PERIOD_MS
#define DAC_TASK_PERIOD_MS 50
#endif

#ifndef DAC_I2C_ADDR_7BIT_TBD
#define DAC_I2C_ADDR_7BIT_TBD 0x48
#endif

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

static void dac_task(void *arg)
{
  (void)arg;

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

  TickType_t last = xTaskGetTickCount();
  uint16_t last_a = 0xFFFF;
  uint16_t last_b = 0xFFFF;

  for (;;)
  {
    vTaskDelayUntil(&last, pdMS_TO_TICKS(DAC_TASK_PERIOD_MS));

    const dac_targets_t t = dac_control_get_targets();

    uint16_t a = t.ld_code;
    uint16_t b = t.tec_code;
    if (t.clamp)
    {
      a = DAC_LD_CODE_SAFE_TBD;
      b = DAC_TEC_CODE_SAFE_TBD;
    }

    if (a != last_a)
    {
      (void)dac80502_i2c_write_reg(&dac, DAC80502_REG_DAC_A_DATA, a);
      last_a = a;
    }
    if (b != last_b)
    {
      (void)dac80502_i2c_write_reg(&dac, DAC80502_REG_DAC_B_DATA, b);
      last_b = b;
    }
  }
}

void dac_task_start(void)
{
  xTaskCreatePinnedToCore(dac_task, "dac", 4096, NULL, 6, NULL, 0);
  ESP_LOGI(TAG, "DAC task started (period=%dms)", (int)DAC_TASK_PERIOD_MS);
}

