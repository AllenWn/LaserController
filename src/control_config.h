#pragma once

#include <stdint.h>

// -----------------------------------------------------------------------------
// Control / setpoint configuration (TBD)
//
// These values define what the firmware writes to the DAC for LD current setpoint
// and TEC temperature setpoint.
//
// IMPORTANT:
// - The mapping from desired physical units (A, °C) to DAC code is board-specific.
// - Until this mapping is confirmed, keep these as raw DAC register codes.
// -----------------------------------------------------------------------------

// DAC codes are 16-bit register values written to DAC80502.
// (Exact effective resolution depends on device family configuration; treat as raw for now.)

#ifndef DAC_LD_CODE_SAFE_TBD
#define DAC_LD_CODE_SAFE_TBD ((uint16_t)0x0000)
#endif

#ifndef DAC_TEC_CODE_SAFE_TBD
#define DAC_TEC_CODE_SAFE_TBD ((uint16_t)0x0000)
#endif

// READY/IDLE target (e.g., maintain temperature setpoint while not firing).
#ifndef DAC_TEC_CODE_READY_TBD
#define DAC_TEC_CODE_READY_TBD ((uint16_t)0x8000)
#endif

// FIRING target (laser current setpoint).
#ifndef DAC_LD_CODE_FIRING_TBD
#define DAC_LD_CODE_FIRING_TBD ((uint16_t)0x1000)
#endif

