#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  SYS_OFF = 0,
  SYS_POWERUP_TEC = 1,
  SYS_POWERUP_LD = 2,
  SYS_READY = 3,
  SYS_FIRING = 4,
  SYS_FAULT_INTERNAL = 5,
  SYS_FAULT_USAGE = 6,
} system_state_t;

typedef enum
{
  LD_MODE_SHUTDOWN = 0,
  LD_MODE_STANDBY = 1,
  LD_MODE_OPERATE = 2,
} ld_mode_t;

typedef struct
{
  // Policy knobs; finalize during system design.
  ld_mode_t ready_ld_mode;
} fsm_config_t;

typedef struct
{
  // Operator intent (debounced).
  bool trigger;

  // Safety supervisor decision (true means all required conditions OK).
  bool permit;

  // Power-domain readiness used by power-up sequencing.
  bool pwr_tec_ready;
  bool pwr_ld_ready;

  // External/system fault inputs (latched behavior handled by FSM).
  bool internal_fault_present;
  bool usage_fault_present;

  // Fault clear requests.
  bool internal_fault_clear;
  bool usage_fault_clear;
} fsm_inputs_t;

typedef struct
{
  system_state_t state;

  // High-level actions requested by FSM; other modules should implement these.
  bool enable_tec_power;
  bool enable_ld_power;
  ld_mode_t ld_mode;

  // Emission command request (still must be gated by safety supervisor).
  bool want_emission;

  // When true, control/setpoint layer must force safe defaults (DAC clamp, etc.).
  bool clamp_setpoints;
} fsm_outputs_t;

void fsm_init(const fsm_config_t *cfg);

system_state_t fsm_get_state(void);
void fsm_force_state(system_state_t state);

// Step the FSM once using current inputs and return outputs.
fsm_outputs_t fsm_step(const fsm_inputs_t *in);
