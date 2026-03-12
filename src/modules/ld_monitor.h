#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

typedef struct
{
  gpio_num_t pwr_ld_pgood_gpio; // digital input
  gpio_num_t ld_lpgd_gpio;      // digital input

  // Optional analog monitor pins (ADC). Keep as GPIOs here; ADC wiring decided later.
  gpio_num_t ld_tmo_gpio; // analog: temperature monitor voltage
  gpio_num_t ld_lio_gpio; // analog: current monitor voltage

  // If false, the analog channel is sampled (if possible) but does not affect .ok.
  bool use_tmo_for_safety;
  bool use_lio_for_safety;

  // ADC voltage read hook.
  //
  // Expected behavior:
  // - return true and set *out_v to the pin voltage (volts) when a valid fresh sample is available
  // - return false if ADC not implemented, misconfigured, or sample unavailable
  //
  // TBD (board-specific): whether there is an external divider/op-amp scaling before the MCU.
  bool (*read_voltage)(gpio_num_t gpio, float *out_v);

  // Active-level configuration (placeholders; verify on hardware).
  bool pwr_ld_pgood_active_high;
  bool ld_lpgd_active_high;

  // ---------------------------------------------------------------------------
  // Analog conversion formulas (TBD: verify against your board's analog scaling)
  // ---------------------------------------------------------------------------
  // LD_TMO: controller temperature monitor voltage -> temperature (°C)
  //
  // Datasheet provides a linear approximation:
  //   T(°C) ≈ 192.5576 − 90.1040 × V_TMO
  //
  // If your PCB scales V_TMO before the MCU ADC, you must adjust the coefficients.
  float tmo_temp_a_c; // default 192.5576
  float tmo_temp_b_c_per_v; // default -90.1040

  // LD_LIO: output current monitor voltage -> current (A)
  //
  // Datasheet:
  //   V_LIO = (I_OUT / 6A) * 2.5V  =>  I_OUT = V_LIO * (6/2.5) = V_LIO * 2.4
  //
  // If your PCB scales V_LIO before the MCU ADC, adjust this factor.
  float lio_amps_per_v; // default 2.4

  // ---------------------------------------------------------------------------
  // Safety thresholds (TBD: define during system safety design)
  // ---------------------------------------------------------------------------
  float temp_max_c;    // over-temp threshold
  float temp_hyst_c;   // hysteresis below max for recovery
  float current_max_a; // over-current threshold
  float current_hyst_a;

  // Debounce: require N consecutive samples to assert fault/recovery.
  uint8_t fault_debounce_samples;
  uint8_t recover_debounce_samples;
} ld_monitor_config_t;

typedef struct
{
  bool pwr_good;
  bool loop_good;
  bool ok;
} ld_monitor_digital_status_t;

typedef struct
{
  bool tmo_valid;
  float tmo_v;
  float temp_c;
  bool temp_ok;

  bool lio_valid;
  float lio_v;
  float current_a;
  bool current_ok;

  bool ok;
} ld_monitor_analog_status_t;

esp_err_t ld_monitor_init(const ld_monitor_config_t *cfg);

// Fast path: read digital "good" lines and compute an OK verdict immediately.
esp_err_t ld_monitor_sample_digital(ld_monitor_digital_status_t *out);

// Slow path: sample analog monitor voltages and evaluate thresholds (if enabled).
esp_err_t ld_monitor_sample_analog(ld_monitor_analog_status_t *out);

// Convenience: sample both and combine into a single OK.
esp_err_t ld_monitor_sample(ld_monitor_digital_status_t *dig, ld_monitor_analog_status_t *ana, bool *out_ok);
