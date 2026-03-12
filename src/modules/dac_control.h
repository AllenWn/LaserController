#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct
{
  uint16_t ld_code;
  uint16_t tec_code;
  bool clamp;
} dac_targets_t;

void dac_control_init(void);
void dac_control_set_targets(uint16_t ld_code, uint16_t tec_code);
void dac_control_set_clamp(bool clamp);
dac_targets_t dac_control_get_targets(void);

