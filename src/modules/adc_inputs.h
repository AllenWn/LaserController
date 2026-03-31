#pragma once

#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_err.h"

esp_err_t adc_inputs_init(void);
bool adc_inputs_read_voltage(gpio_num_t gpio, float *out_v);
