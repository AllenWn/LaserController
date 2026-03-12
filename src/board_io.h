#pragma once

#include <stdbool.h>

#include "esp_err.h"

#include "fsm.h"

// Board-level IO control for safety-critical outputs.
// This is the only place that should directly drive GPIO output pins.

esp_err_t board_io_init(void);

// Apply FSM outputs with safety overrides.
// - If permit is false, emission is forced off (LD in SHUTDOWN).
// - If fault_present is true, outputs are driven to safe state.
esp_err_t board_io_apply_fsm_outputs(const fsm_outputs_t *out, bool permit, bool fault_present);

