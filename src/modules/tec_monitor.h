#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

typedef struct
{
  // Digital inputs
  gpio_num_t pwr_tec_pgood_gpio; // PWR_TEC_GOOD
  gpio_num_t tec_tempgd_gpio;    // TEC_TEMPGD

  // Analog monitor pins (ADC; board-specific)
  gpio_num_t tec_tmo_gpio;  // object temperature monitor voltage
  gpio_num_t tec_itec_gpio; // current monitor voltage
  gpio_num_t tec_vtec_gpio; // TEC voltage monitor voltage

  // If false, the analog channel is sampled (if possible) but does not affect .ok.
  bool use_tmo_for_safety;
  bool use_itec_for_safety;
  bool use_vtec_for_safety;

  // ADC voltage read hook (same semantics as LD monitor).
  bool (*read_voltage)(gpio_num_t gpio, float *out_v);

  // Active-level configuration.
  bool pwr_tec_pgood_active_high;
  bool tec_tempgd_active_high;

  // ---------------------------------------------------------------------------
  // Analog conversion formulas
  // ---------------------------------------------------------------------------
  // TEC_TMO: datasheet notes ~0.1V to 2.5V across default network range. Mapping to °C is board-specific.
  // The datasheet note available in this project does not lock a direct °C formula,
  // so firmware keeps this as a board/calibration-defined linear placeholder:
  //   T(°C) = a + b * V_TMO
  float tmo_temp_a_c;
  float tmo_temp_b_c_per_v;

  // TEC_ITEC: datasheet formula (pin table):
  //   ITEC(A) = (V_ITEC - 1.25) / 0.285
  // If PCB scales V_ITEC before the MCU ADC, adjust these parameters.
  float itec_v_offset_v; // default 1.25
  float itec_v_per_a;    // default 0.285

  // TEC_VTEC: voltage indication; datasheet-level scaling still depends on the
  // board path used in this project.
  // Provide a multiplier placeholder:
  //   V_TEC = V_pin * vtec_scale
  float vtec_scale;

  // ---------------------------------------------------------------------------
  // Safety thresholds (system-level design values; not fixed by chip datasheet)
  // ---------------------------------------------------------------------------
  float temp_max_c;
  float temp_hyst_c;

  float itec_max_a;
  float itec_hyst_a;

  float vtec_max_v;
  float vtec_hyst_v;

  // Debounce: require N consecutive samples to assert fault/recovery.
  uint8_t fault_debounce_samples;
  uint8_t recover_debounce_samples;
} tec_monitor_config_t;

typedef struct
{
  bool pwr_good;
  bool temp_good;
  bool ok;
} tec_monitor_digital_status_t;

typedef struct
{
  bool tmo_valid;
  float tmo_v;
  float temp_c;
  bool temp_ok;

  bool itec_valid;
  float itec_v;
  float itec_a;
  bool itec_ok;

  bool vtec_valid;
  float vtec_v_pin;
  float vtec_v;
  bool vtec_ok;

  bool ok;
} tec_monitor_analog_status_t;

esp_err_t tec_monitor_init(const tec_monitor_config_t *cfg);

esp_err_t tec_monitor_sample_digital(tec_monitor_digital_status_t *out);
esp_err_t tec_monitor_sample_analog(tec_monitor_analog_status_t *out);
esp_err_t tec_monitor_sample(tec_monitor_digital_status_t *dig, tec_monitor_analog_status_t *ana, bool *out_ok);
