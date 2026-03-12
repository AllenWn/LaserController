#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

// Power-domain monitor (PGOOD sampling + basic sequencing helpers).
//
// This module reads PGOOD lines for TEC and LD power domains.
// It does NOT directly control enables (that belongs to control layer / FSM outputs),
// but it can provide "domain_ready" verdicts and simple debounce/timeout handling.

typedef struct
{
  gpio_num_t pwr_tec_pgood_gpio;
  gpio_num_t pwr_ld_pgood_gpio;

  // Active-level configuration (placeholders; verify on hardware).
  bool tec_pgood_active_high;
  bool ld_pgood_active_high;

  // Debounce: require N consecutive samples to assert ready; clear immediately on NOT good.
  uint8_t assert_debounce_samples; // default 1
} pwr_monitor_config_t;

typedef struct
{
  bool tec_pgood;
  bool ld_pgood;

  bool tec_ready;
  bool ld_ready;
} pwr_monitor_status_t;

esp_err_t pwr_monitor_init(const pwr_monitor_config_t *cfg);
esp_err_t pwr_monitor_sample(pwr_monitor_status_t *out);

