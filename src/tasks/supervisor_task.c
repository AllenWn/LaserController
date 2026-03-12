#include "tasks/supervisor_task.h"

#include <stdatomic.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board_io.h"
#include "control_config.h"
#include "fsm.h"
#include "modules/dac_control.h"
#include "system_config.h"
#include "system_status.h"

static const char *TAG = "supervisor";

static atomic_bool s_trigger;

static bool is_stale(TickType_t now, TickType_t updated_at, uint32_t timeout_ms)
{
  const TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
  return (now - updated_at) > timeout_ticks;
}

static uint32_t compute_safety_bits(const system_status_snapshot_t *s, TickType_t now)
{
  uint32_t bits = 0;

  // PWR
  if (s->pwr.updated_at != 0 && !is_stale(now, s->pwr.updated_at, PWR_STATUS_TIMEOUT_MS))
  {
    if (s->pwr.tec_ready)
      bits |= SAFETY_BIT_PWR_TEC_READY;
    if (s->pwr.ld_ready)
      bits |= SAFETY_BIT_PWR_LD_READY;
  }

  // TEC digital
  if (s->tec_dig.updated_at != 0 && !is_stale(now, s->tec_dig.updated_at, TEC_STATUS_TIMEOUT_MS))
  {
    if (s->tec_dig.ok)
      bits |= SAFETY_BIT_TEC_OK;
  }

  // LD digital
  if (s->ld_dig.updated_at != 0 && !is_stale(now, s->ld_dig.updated_at, LD_STATUS_TIMEOUT_MS))
  {
    if (s->ld_dig.ok)
      bits |= SAFETY_BIT_LD_OK;
  }

  // IMU
  if (s->imu.updated_at != 0 && !is_stale(now, s->imu.updated_at, IMU_STATUS_TIMEOUT_MS))
  {
    if (s->imu.valid && s->imu.orientation_ok)
      bits |= SAFETY_BIT_IMU_ORIENT_OK;
  }

  return bits;
}

static bool compute_permit(uint32_t safety_bits)
{
  // Required conditions (v1):
  // - both power domains ready
  // - TEC OK, LD OK
  // - IMU orientation OK
  const uint32_t required =
      (uint32_t)(SAFETY_BIT_PWR_TEC_READY | SAFETY_BIT_PWR_LD_READY | SAFETY_BIT_TEC_OK | SAFETY_BIT_LD_OK |
                 SAFETY_BIT_IMU_ORIENT_OK);

  return (safety_bits & required) == required;
}

static void supervisor_task(void *arg)
{
  (void)arg;
  TickType_t last_wake = xTaskGetTickCount();
  TickType_t last_log = last_wake;
  system_state_t last_state = fsm_get_state();

  for (;;)
  {
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SUPERVISOR_TASK_PERIOD_MS));

    const TickType_t now = xTaskGetTickCount();
    const system_status_snapshot_t snap = system_status_get_snapshot();

    const uint32_t safety_bits = compute_safety_bits(&snap, now);
    const bool permit = compute_permit(safety_bits) && (snap.fault_latch == 0);

    fsm_inputs_t in = {
        .trigger = atomic_load(&s_trigger),
        .permit = permit,
        .fault_present = (snap.fault_latch != 0),
        .fault_clear = false, // TBD: define operator/service clear mechanism
    };
    const fsm_outputs_t out = fsm_step(&in);

    // Apply outputs to hardware (with safety overrides).
    (void)board_io_apply_fsm_outputs(&out, permit, (snap.fault_latch != 0));

    // DAC policy: clamp whenever FSM requests clamp OR safety is not permitting OR fault is present.
    const bool clamp = out.clamp_setpoints || !permit || (snap.fault_latch != 0);
    dac_control_set_clamp(clamp);
    if (out.state == SYS_FIRING && permit && !clamp)
    {
      // TBD: replace with real setpoint selection logic / command interface.
      dac_control_set_targets(DAC_LD_CODE_FIRING_TBD, DAC_TEC_CODE_READY_TBD);
    }
    else
    {
      // Keep TEC regulation setpoint during READY; LD setpoint safe.
      dac_control_set_targets(DAC_LD_CODE_SAFE_TBD, DAC_TEC_CODE_READY_TBD);
    }

    const system_state_t st = out.state;
    if (st != last_state)
    {
      ESP_LOGI(TAG, "State %d -> %d (permit=%d bits=0x%08X faults=0x%08X)", (int)last_state, (int)st, permit ? 1 : 0,
               (unsigned)safety_bits, (unsigned)snap.fault_latch);
      last_state = st;
    }

    if ((now - last_log) >= pdMS_TO_TICKS(STATUS_LOG_PERIOD_MS))
    {
      last_log = now;
      ESP_LOGI(TAG,
               "st=%d trg=%d permit=%d bits=0x%08X faults=0x%08X out{tec=%d ld=%d ld_mode=%d emit=%d clamp=%d} "
               "imu{ok=%d ang=%.1f}",
               (int)st, in.trigger ? 1 : 0, permit ? 1 : 0, (unsigned)safety_bits,
               (unsigned)snap.fault_latch, out.enable_tec_power ? 1 : 0, out.enable_ld_power ? 1 : 0, (int)out.ld_mode,
               out.want_emission ? 1 : 0, out.clamp_setpoints ? 1 : 0, snap.imu.orientation_ok ? 1 : 0,
               (double)snap.imu.angle_from_down_deg);
    }
  }
}

void supervisor_task_start(void)
{
  atomic_store(&s_trigger, false);
  xTaskCreatePinnedToCore(supervisor_task, "supervisor", 4096, NULL, configMAX_PRIORITIES - 1, NULL, 0);
}

void supervisor_set_trigger(bool trigger)
{
  atomic_store(&s_trigger, trigger);
}
