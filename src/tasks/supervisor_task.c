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
#include "tasks/dac_task.h"

static const char *TAG = "supervisor";
// Fault classes:
// - INTERNAL: electrical/control-path faults; force FAULT state and drop EN.
// - USAGE: procedural/safety-envelope faults; block emission but keep power path alive.
static const uint32_t FAULT_LATCH_USAGE_IMU = (1u << 0);
static const uint32_t FAULT_LATCH_INTERNAL_LD_TEC = (1u << 8);
static const uint32_t FAULT_LATCH_INTERNAL_PWR_DROP = (1u << 9);
static const uint32_t FAULT_LATCH_INTERNAL_PWRUP_TIMEOUT = (1u << 10);
static const uint32_t FAULT_MASK_USAGE = (FAULT_LATCH_USAGE_IMU);
static const uint32_t FAULT_MASK_INTERNAL =
    (FAULT_LATCH_INTERNAL_LD_TEC | FAULT_LATCH_INTERNAL_PWR_DROP | FAULT_LATCH_INTERNAL_PWRUP_TIMEOUT);

#ifndef POWERUP_TEC_TIMEOUT_MS
#define POWERUP_TEC_TIMEOUT_MS 1500
#endif

#ifndef POWERUP_LD_TIMEOUT_MS
#define POWERUP_LD_TIMEOUT_MS 1500
#endif

#ifndef FAULT_CLEAR_TRIGGER_RELEASE_SAMPLES
#define FAULT_CLEAR_TRIGGER_RELEASE_SAMPLES 3
#endif

#ifndef FAULT_CLEAR_REQUIRE_IMU_SAFE
#define FAULT_CLEAR_REQUIRE_IMU_SAFE 1
#endif

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
  // Required conditions for emission permit:
  // - both power domains ready
  // - TEC OK, LD OK
  // - IMU orientation OK
  const uint32_t required =
      (uint32_t)(SAFETY_BIT_PWR_TEC_READY | SAFETY_BIT_PWR_LD_READY | SAFETY_BIT_TEC_OK | SAFETY_BIT_LD_OK |
                 SAFETY_BIT_IMU_ORIENT_OK);

  return (safety_bits & required) == required;
}

static bool has_internal_fault(uint32_t fault_latch) { return (fault_latch & FAULT_MASK_INTERNAL) != 0u; }
static bool has_usage_fault(uint32_t fault_latch) { return (fault_latch & FAULT_MASK_USAGE) != 0u; }

static void log_supervisor_snapshot(const char *reason, system_state_t st, const fsm_inputs_t *in, const fsm_outputs_t *out,
                                    uint32_t safety_bits, uint32_t fault_latch, const system_status_snapshot_t *snap,
                                    bool internal_fault_present, bool usage_block, bool permit)
{
  ESP_LOGI(TAG,
           "%s st=%d trg=%d permit=%d bits=0x%08X faults=0x%08X int=%d use=%d "
           "in{tec=%d ld=%d} out{tec=%d ld=%d ld_mode=%d emit=%d clamp=%d} imu{ok=%d ang=%.1f}",
           reason, (int)st, in->trigger ? 1 : 0, permit ? 1 : 0, (unsigned)safety_bits, (unsigned)fault_latch,
           internal_fault_present ? 1 : 0, usage_block ? 1 : 0, in->pwr_tec_ready ? 1 : 0, in->pwr_ld_ready ? 1 : 0,
           out->enable_tec_power ? 1 : 0, out->enable_ld_power ? 1 : 0, (int)out->ld_mode, out->want_emission ? 1 : 0,
           out->clamp_setpoints ? 1 : 0, snap->imu.orientation_ok ? 1 : 0, (double)snap->imu.angle_from_down_deg);
}

static void supervisor_task(void *arg)
{
  (void)arg;
  TickType_t last_wake = xTaskGetTickCount();
  system_state_t last_state = fsm_get_state();
  TickType_t state_enter_tick = last_wake;
  uint8_t trigger_release_cnt = 0;
  bool last_clamp = true;
  uint16_t last_ld_code = 0xFFFF;
  uint16_t last_tec_code = 0xFFFF;
  bool last_trigger = false;
  bool last_permit = false;
  uint32_t last_safety_bits = UINT32_MAX;
  uint32_t last_fault_latch = UINT32_MAX;
  bool last_internal_fault = false;
  bool last_usage_block = false;

  for (;;)
  {
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SUPERVISOR_TASK_PERIOD_MS));

    const TickType_t now = xTaskGetTickCount();
    const system_status_snapshot_t snap = system_status_get_snapshot();

    const uint32_t safety_bits = compute_safety_bits(&snap, now);
    const bool permit_raw = compute_permit(safety_bits);
    const bool trigger = atomic_load(&s_trigger);
    const system_state_t state_before = fsm_get_state();
    const bool pwr_tec_ready = (safety_bits & SAFETY_BIT_PWR_TEC_READY) != 0u;
    const bool pwr_ld_ready = (safety_bits & SAFETY_BIT_PWR_LD_READY) != 0u;
    const bool pwr_ready = pwr_tec_ready && pwr_ld_ready;
    const bool imu_fresh = (snap.imu.updated_at != 0) && !is_stale(now, snap.imu.updated_at, IMU_STATUS_TIMEOUT_MS);
    const bool imu_safe = imu_fresh && snap.imu.valid && snap.imu.orientation_ok;
    const bool tec_ok = (safety_bits & SAFETY_BIT_TEC_OK) != 0u;
    const bool ld_ok = (safety_bits & SAFETY_BIT_LD_OK) != 0u;
    const bool emit_checks_ok = tec_ok && ld_ok && imu_safe;

    uint32_t fault_latch = snap.fault_latch;

    // Fault-latch policy:
    // 1) If safety is lost while FIRING, classify and latch:
    //    - IMU-related => USAGE fault
    //    - LD/TEC-related => INTERNAL fault
    const bool safety_trip_while_firing = (state_before == SYS_FIRING) && !emit_checks_ok;
    if (safety_trip_while_firing)
    {
      uint32_t mask = 0u;
      if (!imu_safe)
      {
        mask |= FAULT_LATCH_USAGE_IMU;
      }
      if (!ld_ok || !tec_ok)
      {
        mask |= FAULT_LATCH_INTERNAL_LD_TEC;
      }
      if (mask != 0u)
      {
        system_status_fault_latch(mask);
        fault_latch |= mask;
      }
    }

    // 2) If power-good is lost after power-up sequence, latch power-drop fault.
    const bool pwr_drop_runtime = (state_before == SYS_READY || state_before == SYS_FIRING) && !pwr_ready;
    if (pwr_drop_runtime)
    {
      system_status_fault_latch(FAULT_LATCH_INTERNAL_PWR_DROP);
      fault_latch |= FAULT_LATCH_INTERNAL_PWR_DROP;
    }

    // 3) Power-up timeout protection.
    const TickType_t tec_timeout = pdMS_TO_TICKS(POWERUP_TEC_TIMEOUT_MS);
    const TickType_t ld_timeout = pdMS_TO_TICKS(POWERUP_LD_TIMEOUT_MS);
    if (state_before == SYS_POWERUP_TEC && (now - state_enter_tick) > tec_timeout)
    {
      system_status_fault_latch(FAULT_LATCH_INTERNAL_PWRUP_TIMEOUT);
      fault_latch |= FAULT_LATCH_INTERNAL_PWRUP_TIMEOUT;
    }
    if (state_before == SYS_POWERUP_LD && (now - state_enter_tick) > ld_timeout)
    {
      system_status_fault_latch(FAULT_LATCH_INTERNAL_PWRUP_TIMEOUT);
      fault_latch |= FAULT_LATCH_INTERNAL_PWRUP_TIMEOUT;
    }

    // Clear policy (v1): operator must release trigger for N consecutive samples.
    if (!trigger)
    {
      if (trigger_release_cnt < 255u)
      {
        trigger_release_cnt++;
      }
    }
    else
    {
      trigger_release_cnt = 0;
    }
    // Clear policy:
    // - Require trigger released for N consecutive samples.
    // - Do NOT gate on permit_raw here, because FAULT drives EN low and that
    //   intentionally makes PGOOD-based permit conditions false.
    // - Optionally require IMU orientation safe before allowing re-arm sequence.
    bool clear_safety_ok = true;
#if FAULT_CLEAR_REQUIRE_IMU_SAFE
    clear_safety_ok = imu_safe;
#endif
    const bool release_ok = (trigger_release_cnt >= FAULT_CLEAR_TRIGGER_RELEASE_SAMPLES);
    const bool internal_present = has_internal_fault(fault_latch);
    const bool usage_present = has_usage_fault(fault_latch);
    const bool clear_internal = internal_present && clear_safety_ok && release_ok;
    const bool clear_usage = usage_present && clear_safety_ok && release_ok;
    if (clear_internal || clear_usage)
    {
      uint32_t clear_mask = 0u;
      if (clear_internal)
      {
        clear_mask |= FAULT_MASK_INTERNAL;
      }
      if (clear_usage)
      {
        clear_mask |= FAULT_MASK_USAGE;
      }
      system_status_fault_clear(clear_mask);
      fault_latch &= ~clear_mask;
    }

    const bool internal_fault_present = has_internal_fault(fault_latch);
    const bool usage_block = has_usage_fault(fault_latch);
    const bool permit = permit_raw && !internal_fault_present && !usage_block;

    fsm_inputs_t in = {
        .trigger = trigger,
        .permit = permit,
        .pwr_tec_ready = pwr_tec_ready,
        .pwr_ld_ready = pwr_ld_ready,
        .internal_fault_present = internal_fault_present,
        .usage_fault_present = usage_block,
        .internal_fault_clear = clear_internal,
        .usage_fault_clear = clear_usage,
    };
    const fsm_outputs_t out = fsm_step(&in);

    // Apply outputs to hardware (with safety overrides).
    (void)board_io_apply_fsm_outputs(&out, permit, internal_fault_present);

    // DAC policy: clamp whenever FSM requests clamp OR safety is not permitting OR fault is present.
    const bool clamp = out.clamp_setpoints || !permit || internal_fault_present;
    uint16_t ld_code = DAC_LD_CODE_SAFE_TBD;
    uint16_t tec_code = DAC_TEC_CODE_READY_TBD;
    if (out.state == SYS_FIRING && permit && !clamp)
    {
      // TBD: replace with real setpoint selection logic / command interface.
      ld_code = DAC_LD_CODE_FIRING_TBD;
    }

    const bool clamp_effective = clamp;

    const bool changed = (clamp_effective != last_clamp) || (ld_code != last_ld_code) || (tec_code != last_tec_code);
    if (changed)
    {
      dac_control_set_clamp(clamp_effective);
      dac_control_set_targets(ld_code, tec_code);
      dac_task_notify_update();
      last_clamp = clamp_effective;
      last_ld_code = ld_code;
      last_tec_code = tec_code;
    }

    const system_state_t st = out.state;
    if (st != last_state)
    {
      ESP_LOGI(TAG, "State %d -> %d", (int)last_state, (int)st);
      log_supervisor_snapshot("state", st, &in, &out, safety_bits, fault_latch, &snap, internal_fault_present,
                              usage_block, permit);
      last_state = st;
      state_enter_tick = now;
    }

    const bool snapshot_changed = (trigger != last_trigger) || (permit != last_permit) || (safety_bits != last_safety_bits) ||
                                  (fault_latch != last_fault_latch) || (internal_fault_present != last_internal_fault) ||
                                  (usage_block != last_usage_block);
    if (snapshot_changed)
    {
      log_supervisor_snapshot("snap", st, &in, &out, safety_bits, fault_latch, &snap, internal_fault_present,
                              usage_block, permit);
      last_trigger = trigger;
      last_permit = permit;
      last_safety_bits = safety_bits;
      last_fault_latch = fault_latch;
      last_internal_fault = internal_fault_present;
      last_usage_block = usage_block;
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
