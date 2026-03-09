#include "state_machine.h"

#include <stdatomic.h>

#include "esp_log.h"
#include "safety.h"

static const char *TAG = "sm";

static atomic_int s_state;
static atomic_bool s_arm_req;
static atomic_bool s_trigger_req;

void state_machine_init(void)
{
  atomic_store(&s_state, SYS_OFF);
  atomic_store(&s_arm_req, false);
  atomic_store(&s_trigger_req, false);
  safety_set_laser_command(false);
}

system_state_t state_machine_get_state(void)
{
  return (system_state_t)atomic_load(&s_state);
}

void state_machine_set_arm_request(bool arm)
{
  atomic_store(&s_arm_req, arm);
}

void state_machine_set_trigger_request(bool trigger)
{
  atomic_store(&s_trigger_req, trigger);
}

static void transition_to(system_state_t next)
{
  const system_state_t prev = (system_state_t)atomic_exchange(&s_state, next);
  if (prev != next)
  {
    ESP_LOGI(TAG, "State %d -> %d", (int)prev, (int)next);
  }
}

void state_machine_step(bool permit)
{
  const system_state_t state = state_machine_get_state();
  const bool arm = atomic_load(&s_arm_req);
  const bool trigger = atomic_load(&s_trigger_req);

  // Global safety rule: if permit is lost, stop emitting immediately.
  if (!permit)
  {
    safety_set_laser_command(false);
    if (state == SYS_FIRING)
    {
      transition_to(SYS_ARMED);
    }
  }

  switch (state)
  {
  case SYS_OFF:
    safety_set_laser_command(false);
    if (arm)
    {
      transition_to(SYS_ARMED);
    }
    break;

  case SYS_ARMED:
    safety_set_laser_command(false);
    if (!arm)
    {
      transition_to(SYS_OFF);
      break;
    }
    if (permit && trigger)
    {
      transition_to(SYS_FIRING);
      safety_set_laser_command(true);
    }
    break;

  case SYS_FIRING:
    if (!arm)
    {
      safety_set_laser_command(false);
      transition_to(SYS_OFF);
      break;
    }
    if (!trigger)
    {
      safety_set_laser_command(false);
      transition_to(SYS_ARMED);
      break;
    }
    // Permit loss handled above.
    break;

  case SYS_FAULT:
  default:
    safety_set_laser_command(false);
    // Reset/clear policy TBD. Remains latched.
    break;
  }

  // Force a fault state if any forced-off reason exists.
  if (safety_get_force_off_reasons() != 0)
  {
    safety_set_laser_command(false);
    transition_to(SYS_FAULT);
  }
}

