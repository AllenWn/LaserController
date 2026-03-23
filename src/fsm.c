#include "fsm.h"

#include <stdatomic.h>
#include <string.h>

static atomic_int s_state;
static fsm_config_t s_cfg;

static void transition_to(system_state_t next)
{
  atomic_store(&s_state, (int)next);
}

void fsm_init(const fsm_config_t *cfg)
{
  transition_to(SYS_OFF);
  memset(&s_cfg, 0, sizeof(s_cfg));
  if (cfg)
  {
    s_cfg = *cfg;
  }
}

system_state_t fsm_get_state(void)
{
  return (system_state_t)atomic_load(&s_state);
}

void fsm_force_state(system_state_t state)
{
  transition_to(state);
}

static fsm_outputs_t outputs_for_state(system_state_t state)
{
  fsm_outputs_t o = {0};
  o.state = state;

  switch (state)
  {
  case SYS_OFF:
    o.enable_tec_power = false;
    o.enable_ld_power = false;
    o.ld_mode = LD_MODE_SHUTDOWN;
    o.want_emission = false;
    o.clamp_setpoints = true;
    break;

  case SYS_POWERUP_TEC:
    o.enable_tec_power = true;
    o.enable_ld_power = false;
    o.ld_mode = LD_MODE_SHUTDOWN;
    o.want_emission = false;
    o.clamp_setpoints = true;
    break;

  case SYS_POWERUP_LD:
    o.enable_tec_power = true;
    o.enable_ld_power = true;
    o.ld_mode = LD_MODE_SHUTDOWN;
    o.want_emission = false;
    o.clamp_setpoints = true;
    break;

  case SYS_READY:
    o.enable_tec_power = true;
    o.enable_ld_power = true;
    o.ld_mode = s_cfg.ready_ld_mode;
    o.want_emission = false;
    o.clamp_setpoints = true;
    break;

  case SYS_FIRING:
    // Power on; allow setpoints. Emission still requires safety supervisor gating.
    o.enable_tec_power = true;
    o.enable_ld_power = true;
    o.ld_mode = LD_MODE_OPERATE;
    o.want_emission = true;
    o.clamp_setpoints = false;
    break;

  case SYS_FAULT:
  default:
    o.enable_tec_power = false;
    o.enable_ld_power = false;
    o.ld_mode = LD_MODE_SHUTDOWN;
    o.want_emission = false;
    o.clamp_setpoints = true;
    break;
  }

  return o;
}

fsm_outputs_t fsm_step(const fsm_inputs_t *in)
{
  const fsm_inputs_t i = in ? *in : (fsm_inputs_t){0};
  system_state_t state = fsm_get_state();

  // Fault always wins.
  if (i.fault_present && state != SYS_FAULT)
  {
    transition_to(SYS_FAULT);
    state = SYS_FAULT;
  }

  switch (state)
  {
  case SYS_OFF:
    // No explicit ARM input in this design: auto-start power-up sequence.
    if (!i.fault_present)
    {
      transition_to(SYS_POWERUP_TEC);
      state = SYS_POWERUP_TEC;
    }
    break;

  case SYS_POWERUP_TEC:
    if (i.pwr_tec_ready)
    {
      transition_to(SYS_POWERUP_LD);
      state = SYS_POWERUP_LD;
    }
    break;

  case SYS_POWERUP_LD:
    if (i.pwr_ld_ready)
    {
      transition_to(SYS_READY);
      state = SYS_READY;
    }
    break;

  case SYS_READY:
    if (i.permit && i.trigger)
    {
      transition_to(SYS_FIRING);
      state = SYS_FIRING;
    }
    break;

  case SYS_FIRING:
    // Any loss of intent or permit immediately stops firing.
    if (!i.trigger || !i.permit)
    {
      transition_to(SYS_READY);
      state = SYS_READY;
      break;
    }
    break;

  case SYS_FAULT:
  default:
    // Latched until an explicit clear is requested AND the fault input is no longer present.
    if (i.fault_clear && !i.fault_present)
    {
      transition_to(SYS_OFF);
      state = SYS_OFF;
    }
    break;
  }

  return outputs_for_state(state);
}
