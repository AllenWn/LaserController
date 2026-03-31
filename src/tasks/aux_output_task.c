#include "tasks/aux_output_task.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "drivers/drv2605_i2c.h"
#include "drivers/mcp23017_i2c.h"
#include "drivers/tlc59116_i2c.h"
#include "modules/aux_i2c_bus.h"

static const char *TAG = "aux_out";

#ifndef MCP23017_ADDR_7BIT
#define MCP23017_ADDR_7BIT 0x20
#endif

#ifndef AUX_TLC59116_ADDR_7BIT
#define AUX_TLC59116_ADDR_7BIT 0x60
#endif

#ifndef AUX_DRV2605_ADDR_7BIT
#define AUX_DRV2605_ADDR_7BIT 0x5A
#endif

static esp_err_t aux_expander_set_lra_enable(const mcp23017_i2c_t *mcp, bool enable)
{
  esp_err_t err = mcp23017_i2c_write_reg8(mcp, MCP23017_REG_IODIRB, 0xEF);
  if (err != ESP_OK)
  {
    return err;
  }

  const uint8_t olatb = enable ? (uint8_t)(1u << 4) : 0x00;
  return mcp23017_i2c_write_reg8(mcp, MCP23017_REG_OLATB, olatb);
}

static void aux_output_task(void *arg)
{
  (void)arg;

  if (!aux_i2c_bus_config_is_valid())
  {
    ESP_LOGW(TAG, "Aux output task disabled: Auxiliary I2C host pins not locked yet");
    vTaskDelete(NULL);
    return;
  }

  ESP_ERROR_CHECK(aux_i2c_bus_init());

  tlc59116_i2c_t led = {0};
  drv2605_i2c_t haptic = {0};
  mcp23017_i2c_t mcp = {0};

  (void)tlc59116_i2c_init(&led, aux_i2c_bus_port(), AUX_TLC59116_ADDR_7BIT);
  (void)drv2605_i2c_init(&haptic, aux_i2c_bus_port(), AUX_DRV2605_ADDR_7BIT);
  (void)mcp23017_i2c_init(&mcp, aux_i2c_bus_port(), MCP23017_ADDR_7BIT);

  if (aux_i2c_bus_lock(pdMS_TO_TICKS(100)))
  {
    (void)aux_expander_set_lra_enable(&mcp, false);
    (void)drv2605_i2c_enter_standby(&haptic);
    (void)drv2605_i2c_set_rtp(&haptic, 0x00);
    (void)tlc59116_i2c_all_off(&led);
    aux_i2c_bus_unlock();
  }
  else
  {
    ESP_LOGW(TAG, "Aux output init skipped: I2C bus lock timeout");
  }

  ESP_LOGI(TAG, "Aux output task initialized (LED driver + haptic driver kept safe/off)");

  for (;;)
  {
    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  }
}

void aux_output_task_start(void)
{
  xTaskCreatePinnedToCore(aux_output_task, "aux_out", 4096, NULL, 7, NULL, 0);
  ESP_LOGI(TAG, "Aux output task started");
}
