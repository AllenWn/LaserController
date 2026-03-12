#include "modules/dac_control.h"

#include "portmacro.h"

static dac_targets_t s_targets;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

void dac_control_init(void)
{
  portENTER_CRITICAL(&s_lock);
  s_targets.ld_code = 0;
  s_targets.tec_code = 0;
  s_targets.clamp = true;
  portEXIT_CRITICAL(&s_lock);
}

void dac_control_set_targets(uint16_t ld_code, uint16_t tec_code)
{
  portENTER_CRITICAL(&s_lock);
  s_targets.ld_code = ld_code;
  s_targets.tec_code = tec_code;
  portEXIT_CRITICAL(&s_lock);
}

void dac_control_set_clamp(bool clamp)
{
  portENTER_CRITICAL(&s_lock);
  s_targets.clamp = clamp;
  portEXIT_CRITICAL(&s_lock);
}

dac_targets_t dac_control_get_targets(void)
{
  dac_targets_t t;
  portENTER_CRITICAL(&s_lock);
  t = s_targets;
  portEXIT_CRITICAL(&s_lock);
  return t;
}

