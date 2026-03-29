#include "modules/aux_i2c_bus.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "app_config.h"
#include "driver/i2c.h"

#ifndef AUX_I2C_PORT
#define AUX_I2C_PORT I2C_NUM_1
#endif

static SemaphoreHandle_t s_aux_i2c_mutex;
static bool s_aux_i2c_init_done;

bool aux_i2c_bus_config_is_valid(void)
{
  return ((int)AUX_I2C_SDA_GPIO >= 0) && ((int)AUX_I2C_SCL_GPIO >= 0);
}

esp_err_t aux_i2c_bus_init(void)
{
  if (!aux_i2c_bus_config_is_valid())
  {
    return ESP_ERR_INVALID_STATE;
  }

  if (!s_aux_i2c_mutex)
  {
    s_aux_i2c_mutex = xSemaphoreCreateMutex();
    if (!s_aux_i2c_mutex)
    {
      return ESP_ERR_NO_MEM;
    }
  }

  if (s_aux_i2c_init_done)
  {
    return ESP_OK;
  }

  const i2c_config_t conf = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = AUX_I2C_SDA_GPIO,
      .scl_io_num = AUX_I2C_SCL_GPIO,
      .sda_pullup_en = GPIO_PULLUP_DISABLE,
      .scl_pullup_en = GPIO_PULLUP_DISABLE,
      .master.clk_speed = 400000,
  };

  esp_err_t err = i2c_param_config(AUX_I2C_PORT, &conf);
  if (err != ESP_OK)
  {
    return err;
  }

  err = i2c_driver_install(AUX_I2C_PORT, conf.mode, 0, 0, 0);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
  {
    return err;
  }

  s_aux_i2c_init_done = true;
  return ESP_OK;
}

i2c_port_t aux_i2c_bus_port(void) { return AUX_I2C_PORT; }

bool aux_i2c_bus_lock(TickType_t timeout_ticks)
{
  if (!s_aux_i2c_mutex)
  {
    return false;
  }
  return xSemaphoreTake(s_aux_i2c_mutex, timeout_ticks) == pdTRUE;
}

void aux_i2c_bus_unlock(void)
{
  if (s_aux_i2c_mutex)
  {
    (void)xSemaphoreGive(s_aux_i2c_mutex);
  }
}
