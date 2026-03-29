#include "tasks/aux_button_task.h"

#include <stdbool.h>
#include <stdint.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "driver/gpio.h"
#include "drivers/mcp23017_i2c.h"
#include "modules/aux_i2c_bus.h"
#include "tasks/supervisor_task.h"

static const char *TAG = "aux_btn";

#ifndef AUX_I2C_SCL_GPIO
#define AUX_I2C_SCL_GPIO GPIO_NUM_NC
#endif

#ifndef AUX_I2C_SDA_GPIO
#define AUX_I2C_SDA_GPIO GPIO_NUM_NC
#endif

#ifndef AUX_GPIO_INT_A_GPIO
#define AUX_GPIO_INT_A_GPIO GPIO_NUM_NC
#endif

#ifndef MCP23017_ADDR_7BIT
#define MCP23017_ADDR_7BIT 0x20
#endif

#ifndef AUX_BUTTON_TASK_PERIOD_MS
#define AUX_BUTTON_TASK_PERIOD_MS 5
#endif

#ifndef AUX_TRIGGER_DEBOUNCE_SAMPLES
#define AUX_TRIGGER_DEBOUNCE_SAMPLES 3
#endif

// SW1 on the auxiliary board pulls GPA0 and GPA1 low together when pressed.
// Use both channels as the "laser trigger pressed" condition.
static uint64_t gpio_mask_or_zero(gpio_num_t gpio)
{
  const int gpio_num = (int)gpio;
  if (gpio_num < 0)
  {
    return 0;
  }
  return (1ULL << (unsigned)gpio_num);
}

static esp_err_t mcp23017_configure_for_buttons(const mcp23017_i2c_t *mcp)
{
  esp_err_t err = mcp23017_i2c_write_reg8(mcp, MCP23017_REG_IOCON, 0x00);
  if (err != ESP_OK)
    return err;

  // GPA0..GPA3 are buttons on the Auxiliary board; leave the whole A port as inputs.
  err = mcp23017_i2c_write_reg8(mcp, MCP23017_REG_IODIRA, 0xFF);
  if (err != ESP_OK)
    return err;

  // GPB4 is reserved for LRA_EN output; all other B pins remain inputs.
  err = mcp23017_i2c_write_reg8(mcp, MCP23017_REG_IODIRB, 0xEF);
  if (err != ESP_OK)
    return err;

  // Buttons already have external pull-ups on the Auxiliary board.
  err = mcp23017_i2c_write_reg8(mcp, MCP23017_REG_GPPUA, 0x00);
  if (err != ESP_OK)
    return err;

  err = mcp23017_i2c_write_reg8(mcp, MCP23017_REG_GPPUB, 0x00);
  if (err != ESP_OK)
    return err;

  err = mcp23017_i2c_write_reg8(mcp, MCP23017_REG_OLATB, 0x00);
  if (err != ESP_OK)
    return err;

  return ESP_OK;
}

static bool decode_trigger_pressed(uint8_t gpioa)
{
  const bool gpa0_low = (gpioa & (1u << 0)) == 0u;
  const bool gpa1_low = (gpioa & (1u << 1)) == 0u;
  return gpa0_low && gpa1_low;
}

static void aux_button_task(void *arg)
{
  (void)arg;

  if (!aux_i2c_bus_config_is_valid())
  {
    ESP_LOGW(TAG, "Aux button task disabled: AUX_I2C_SCL_GPIO/AUX_I2C_SDA_GPIO not locked yet");
    supervisor_set_trigger(false);
    vTaskDelete(NULL);
    return;
  }

  if ((int)AUX_GPIO_INT_A_GPIO >= 0)
  {
    gpio_config_t int_cfg = {
        .pin_bit_mask = gpio_mask_or_zero(AUX_GPIO_INT_A_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    (void)gpio_config(&int_cfg);
  }

  ESP_ERROR_CHECK(aux_i2c_bus_init());

  mcp23017_i2c_t mcp = {0};
  ESP_ERROR_CHECK(mcp23017_i2c_init(&mcp, aux_i2c_bus_port(), MCP23017_ADDR_7BIT));
  if (aux_i2c_bus_lock(pdMS_TO_TICKS(100)))
  {
    ESP_ERROR_CHECK(mcp23017_configure_for_buttons(&mcp));
    aux_i2c_bus_unlock();
  }
  else
  {
    ESP_LOGW(TAG, "Aux button task disabled: failed to lock auxiliary I2C bus at startup");
    supervisor_set_trigger(false);
    vTaskDelete(NULL);
    return;
  }

  uint8_t gpioa = 0xFF;
  if (aux_i2c_bus_lock(pdMS_TO_TICKS(20)))
  {
    (void)mcp23017_i2c_read_reg8(&mcp, MCP23017_REG_GPIOA, &gpioa);
    aux_i2c_bus_unlock();
  }

  bool stable = decode_trigger_pressed(gpioa);
  bool last_raw = stable;
  uint8_t same_cnt = 0;
  supervisor_set_trigger(stable);

  TickType_t last = xTaskGetTickCount();
  for (;;)
  {
    vTaskDelayUntil(&last, pdMS_TO_TICKS(AUX_BUTTON_TASK_PERIOD_MS));

    if (!aux_i2c_bus_lock(pdMS_TO_TICKS(10)))
    {
      ESP_LOGW(TAG, "Aux I2C bus lock timeout");
      continue;
    }

    esp_err_t err = mcp23017_i2c_read_reg8(&mcp, MCP23017_REG_GPIOA, &gpioa);
    aux_i2c_bus_unlock();
    if (err != ESP_OK)
    {
      ESP_LOGW(TAG, "MCP23017 read failed: %s", esp_err_to_name(err));
      continue;
    }

    const bool raw = decode_trigger_pressed(gpioa);
    if (raw == last_raw)
    {
      if (same_cnt < 255u)
      {
        same_cnt++;
      }
    }
    else
    {
      last_raw = raw;
      same_cnt = 0;
    }

    if (same_cnt >= AUX_TRIGGER_DEBOUNCE_SAMPLES && stable != raw)
    {
      stable = raw;
      supervisor_set_trigger(stable);
      ESP_LOGI(TAG, "Aux trigger=%d (GPIOA=0x%02X)", stable ? 1 : 0, gpioa);
    }
  }
}

void aux_button_task_start(void)
{
  xTaskCreatePinnedToCore(aux_button_task, "aux_btn", 4096, NULL, 11, NULL, 0);
  ESP_LOGI(TAG, "Aux button task started");
}
