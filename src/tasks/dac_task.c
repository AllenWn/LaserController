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

#ifndef DAC_I2C_ADDR_7BIT
#define DAC_I2C_ADDR_7BIT 0x48
#endif

// DAC80502 initialization for a fixed 0-2.5 V output span on 3.3 V VDD:
// - CONFIG=0x0000 keeps the internal reference enabled and powers up outputs.
// - GAIN=0x0103 sets REFDIV=1 and keeps BUF-x-GAIN enabled on both channels.
//   With the internal 2.5 V reference, that yields an effective 0-2.5 V span:
//     VOUT = CODE / 65536 * (2.5 V / 2) * 2 = CODE / 65536 * 2.5 V
// This makes channel code-to-voltage mapping explicit for bring-up:
// - 0x0000 -> ~0.0 V
// - 0x8000 -> ~1.25 V
// - 0xFFFF -> ~2.5 V
#ifndef DAC80502_CONFIG_INIT_VALUE
#define DAC80502_CONFIG_INIT_VALUE ((uint16_t)0x0000)
#endif

#ifndef DAC80502_GAIN_INIT_VALUE
#define DAC80502_GAIN_INIT_VALUE ((uint16_t)0x0103)
#endif

static TaskHandle_t s_dac_task;
static bool s_last_logged_valid;
static uint16_t s_last_logged_a;
static uint16_t s_last_logged_b;
static bool s_last_logged_clamp;
static esp_err_t s_last_logged_err_a = ESP_FAIL;
static esp_err_t s_last_logged_err_b = ESP_FAIL;

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

static esp_err_t dac_write_with_retry(const dac80502_i2c_t *dac, dac80502_reg_t reg, uint16_t value, uint32_t attempts,
                                      uint32_t delay_ms)
{
  esp_err_t err = ESP_FAIL;
  for (uint32_t attempt = 0; attempt < attempts; ++attempt)
  {
    err = dac80502_i2c_write_reg(dac, reg, value);
    if (err == ESP_OK)
    {
      return ESP_OK;
    }
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
  }
  return err;
}

static esp_err_t dac_read_with_retry(const dac80502_i2c_t *dac, dac80502_reg_t reg, uint16_t *out_value, uint32_t attempts,
                                     uint32_t delay_ms)
{
  esp_err_t err = ESP_FAIL;
  for (uint32_t attempt = 0; attempt < attempts; ++attempt)
  {
    err = dac80502_i2c_read_reg(dac, reg, out_value);
    if (err == ESP_OK)
    {
      return ESP_OK;
    }
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
  }
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

  esp_err_t err_a = ESP_OK;
  esp_err_t err_b = ESP_OK;

  if (a != *last_a)
  {
    err_a = dac80502_i2c_write_reg(dac, DAC80502_REG_DAC_A_DATA, a);
    if (err_a == ESP_OK)
    {
      *last_a = a;
    }
  }
  if (b != *last_b)
  {
    err_b = dac80502_i2c_write_reg(dac, DAC80502_REG_DAC_B_DATA, b);
    if (err_b == ESP_OK)
    {
      *last_b = b;
    }
  }

  const bool changed = !s_last_logged_valid || s_last_logged_a != a || s_last_logged_b != b || s_last_logged_clamp != t.clamp ||
                       s_last_logged_err_a != err_a || s_last_logged_err_b != err_b;
  if (changed)
  {
    ESP_LOGI(TAG, "apply clamp=%d dac_a=0x%04X(%s) dac_b=0x%04X(%s)", t.clamp ? 1 : 0, a,
             esp_err_to_name(err_a), b, esp_err_to_name(err_b));
    s_last_logged_valid = true;
    s_last_logged_a = a;
    s_last_logged_b = b;
    s_last_logged_clamp = t.clamp;
    s_last_logged_err_a = err_a;
    s_last_logged_err_b = err_b;
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
  (void)dac80502_i2c_init(&dac, I2C_NUM_0, DAC_I2C_ADDR_7BIT);

  // Give the external DAC board a short settle window after reset / bus init
  // before the first write. The DAC EVM consistently responds to the next write
  // even when the immediate startup write times out.
  vTaskDelay(pdMS_TO_TICKS(100));

  // Configure the DAC into a known 0-2.5 V output range before writing channel
  // data so the later code-to-voltage relationship is deterministic.
  const esp_err_t cfg_wr = dac_write_with_retry(&dac, DAC80502_REG_CONFIG, DAC80502_CONFIG_INIT_VALUE, 5, 50);
  uint16_t cfg_rb = 0xFFFF;
  const esp_err_t cfg_rd = dac_read_with_retry(&dac, DAC80502_REG_CONFIG, &cfg_rb, 5, 50);
  ESP_LOGI(TAG, "init config=0x%04X write=%s read=%s rb=0x%04X", DAC80502_CONFIG_INIT_VALUE, esp_err_to_name(cfg_wr),
           esp_err_to_name(cfg_rd), cfg_rb);

  const esp_err_t gain_wr = dac_write_with_retry(&dac, DAC80502_REG_GAIN, DAC80502_GAIN_INIT_VALUE, 5, 50);
  uint16_t gain_rb = 0xFFFF;
  const esp_err_t gain_rd = dac_read_with_retry(&dac, DAC80502_REG_GAIN, &gain_rb, 5, 50);
  ESP_LOGI(TAG, "init gain=0x%04X write=%s read=%s rb=0x%04X", DAC80502_GAIN_INIT_VALUE, esp_err_to_name(gain_wr),
           esp_err_to_name(gain_rd), gain_rb);

  // Force safe defaults at startup.
  const esp_err_t init_a = dac_write_with_retry(&dac, DAC80502_REG_DAC_A_DATA, DAC_LD_CODE_SAFE_TBD, 5, 50);
  const esp_err_t init_b = dac_write_with_retry(&dac, DAC80502_REG_DAC_B_DATA, DAC_TEC_CODE_SAFE_TBD, 5, 50);
  uint16_t rb_a = 0xFFFF;
  uint16_t rb_b = 0xFFFF;
  const esp_err_t rd_a = dac_read_with_retry(&dac, DAC80502_REG_DAC_A_DATA, &rb_a, 5, 50);
  const esp_err_t rd_b = dac_read_with_retry(&dac, DAC80502_REG_DAC_B_DATA, &rb_b, 5, 50);
  ESP_LOGI(TAG,
           "startup dac_a=0x%04X(%s/%s rb=0x%04X) dac_b=0x%04X(%s/%s rb=0x%04X)",
           DAC_LD_CODE_SAFE_TBD, esp_err_to_name(init_a), esp_err_to_name(rd_a), rb_a, DAC_TEC_CODE_SAFE_TBD,
           esp_err_to_name(init_b), esp_err_to_name(rd_b), rb_b);

  uint16_t last_a = (init_a == ESP_OK) ? DAC_LD_CODE_SAFE_TBD : (uint16_t)0xFFFF;
  uint16_t last_b = (init_b == ESP_OK) ? DAC_TEC_CODE_SAFE_TBD : (uint16_t)0xFFFF;

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
