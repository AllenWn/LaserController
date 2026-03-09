#pragma once

#include <stdbool.h>

typedef enum
{
  SYS_OFF = 0,
  SYS_ARMED = 1,
  SYS_FIRING = 2,
  SYS_FAULT = 3,
} system_state_t;

void state_machine_init(void);
system_state_t state_machine_get_state(void);

// UI task calls these with debounced button states.
void state_machine_set_arm_request(bool arm);
void state_machine_set_trigger_request(bool trigger);

// Safety task calls this to advance transitions based on permit.
void state_machine_step(bool permit);

