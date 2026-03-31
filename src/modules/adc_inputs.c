#include "modules/adc_inputs.h"

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"

typedef struct
{
  gpio_num_t gpio;
  adc_channel_t channel;
} adc_input_map_t;

static const adc_input_map_t s_adc1_map[] = {
    {GPIO_NUM_1, ADC_CHANNEL_0},
    {GPIO_NUM_2, ADC_CHANNEL_1},
    {GPIO_NUM_8, ADC_CHANNEL_7},
    {GPIO_NUM_9, ADC_CHANNEL_8},
    {GPIO_NUM_10, ADC_CHANNEL_9},
};

static adc_oneshot_unit_handle_t s_adc1_handle;
static adc_cali_handle_t s_adc1_cali;
static bool s_adc_inited;

static bool map_gpio_to_adc1(gpio_num_t gpio, adc_channel_t *out_channel)
{
  if (!out_channel)
  {
    return false;
  }

  for (size_t index = 0; index < (sizeof(s_adc1_map) / sizeof(s_adc1_map[0])); ++index)
  {
    if (s_adc1_map[index].gpio == gpio)
    {
      *out_channel = s_adc1_map[index].channel;
      return true;
    }
  }
  return false;
}

esp_err_t adc_inputs_init(void)
{
  if (s_adc_inited)
  {
    return ESP_OK;
  }

  adc_oneshot_unit_init_cfg_t init_cfg = {
      .unit_id = ADC_UNIT_1,
  };
  ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&init_cfg, &s_adc1_handle), "adc_in", "adc unit init failed");

  adc_oneshot_chan_cfg_t chan_cfg = {
      .bitwidth = ADC_BITWIDTH_DEFAULT,
      .atten = ADC_ATTEN_DB_12,
  };

  for (size_t index = 0; index < (sizeof(s_adc1_map) / sizeof(s_adc1_map[0])); ++index)
  {
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(s_adc1_handle, s_adc1_map[index].channel, &chan_cfg),
                        "adc_in", "adc channel config failed");
  }

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
  adc_cali_curve_fitting_config_t cali_cfg = {
      .unit_id = ADC_UNIT_1,
      .atten = ADC_ATTEN_DB_12,
      .bitwidth = ADC_BITWIDTH_DEFAULT,
  };
  (void)adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_adc1_cali);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
  adc_cali_line_fitting_config_t cali_cfg = {
      .unit_id = ADC_UNIT_1,
      .atten = ADC_ATTEN_DB_12,
      .bitwidth = ADC_BITWIDTH_DEFAULT,
  };
  (void)adc_cali_create_scheme_line_fitting(&cali_cfg, &s_adc1_cali);
#endif

  s_adc_inited = true;
  return ESP_OK;
}

bool adc_inputs_read_voltage(gpio_num_t gpio, float *out_v)
{
  if (!s_adc_inited || !out_v)
  {
    return false;
  }

  adc_channel_t channel;
  if (!map_gpio_to_adc1(gpio, &channel))
  {
    return false;
  }

  int raw = 0;
  if (adc_oneshot_read(s_adc1_handle, channel, &raw) != ESP_OK)
  {
    return false;
  }

  int millivolts = 0;
  if (s_adc1_cali && adc_cali_raw_to_voltage(s_adc1_cali, raw, &millivolts) == ESP_OK)
  {
    *out_v = ((float)millivolts) / 1000.0f;
    return true;
  }

  // Fallback approximation for 12 dB attenuation on ESP32-S3 when calibration
  // data is unavailable. Good enough for bring-up visibility, not final accuracy.
  *out_v = ((float)raw / 4095.0f) * 2.45f;
  return true;
}
