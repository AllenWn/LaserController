#include "system_status.h"

#include <string.h>

#include "portmacro.h"

static system_status_snapshot_t s_status;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

void system_status_init(void)
{
  portENTER_CRITICAL(&s_lock);
  memset(&s_status, 0, sizeof(s_status));
  portEXIT_CRITICAL(&s_lock);
}

void system_status_update_pwr(bool tec_ready, bool ld_ready, TickType_t now)
{
  portENTER_CRITICAL(&s_lock);
  s_status.pwr.tec_ready = tec_ready;
  s_status.pwr.ld_ready = ld_ready;
  s_status.pwr.updated_at = now;
  portEXIT_CRITICAL(&s_lock);
}

void system_status_update_ld_digital(const ld_digital_status_t *st, TickType_t now)
{
  if (!st)
    return;
  portENTER_CRITICAL(&s_lock);
  s_status.ld_dig = *st;
  s_status.ld_dig.updated_at = now;
  portEXIT_CRITICAL(&s_lock);
}

void system_status_update_tec_digital(const tec_digital_status_t *st, TickType_t now)
{
  if (!st)
    return;
  portENTER_CRITICAL(&s_lock);
  s_status.tec_dig = *st;
  s_status.tec_dig.updated_at = now;
  portEXIT_CRITICAL(&s_lock);
}

void system_status_update_ld_analog(const ld_analog_status_t *st, TickType_t now)
{
  if (!st)
    return;
  portENTER_CRITICAL(&s_lock);
  s_status.ld_ana = *st;
  s_status.ld_ana.updated_at = now;
  portEXIT_CRITICAL(&s_lock);
}

void system_status_update_tec_analog(const tec_analog_status_t *st, TickType_t now)
{
  if (!st)
    return;
  portENTER_CRITICAL(&s_lock);
  s_status.tec_ana = *st;
  s_status.tec_ana.updated_at = now;
  portEXIT_CRITICAL(&s_lock);
}

void system_status_update_imu(const imu_status_t *st, TickType_t now)
{
  if (!st)
    return;
  portENTER_CRITICAL(&s_lock);
  s_status.imu = *st;
  s_status.imu.updated_at = now;
  portEXIT_CRITICAL(&s_lock);
}

void system_status_fault_latch(uint32_t fault_mask)
{
  if (fault_mask == 0)
    return;
  portENTER_CRITICAL(&s_lock);
  s_status.fault_latch |= fault_mask;
  portEXIT_CRITICAL(&s_lock);
}

void system_status_fault_clear(uint32_t fault_mask)
{
  portENTER_CRITICAL(&s_lock);
  s_status.fault_latch &= ~fault_mask;
  portEXIT_CRITICAL(&s_lock);
}

system_status_snapshot_t system_status_get_snapshot(void)
{
  system_status_snapshot_t snap;
  portENTER_CRITICAL(&s_lock);
  snap = s_status;
  portEXIT_CRITICAL(&s_lock);
  return snap;
}
