#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"

typedef enum
{
  SAFETY_BIT_TEC_READY = (1u << 0),
  SAFETY_BIT_TEC_OK = (1u << 1),
  SAFETY_BIT_LD_OK = (1u << 2),
  SAFETY_BIT_ORIENTATION_OK = (1u << 3),
  SAFETY_BIT_DISTANCE_OK = (1u << 4),
  SAFETY_BIT_INTERLOCK_OK = (1u << 5),
} safety_bit_t;

typedef enum
{
  LASER_FORCE_NONE = 0,
  LASER_FORCE_LD_FAULT = (1u << 0),
  LASER_FORCE_TEC_FAULT = (1u << 1),
  LASER_FORCE_ESTOP = (1u << 2),
} laser_force_off_reason_t;

void safety_init(void);

// Sensor/logic tasks call these to maintain safety conditions (fail-safe default is false).
void safety_set_condition(safety_bit_t bit, bool ok);

// State machine calls this to request emission; safety logic still gates with permit + faults.
void safety_set_laser_command(bool want_on);

// Returns true only if all safety conditions are satisfied AND no forced-off reason exists.
bool safety_get_laser_permit(void);

// Returns the current desired command from the state machine.
bool safety_get_laser_command(void);

// ISR or tasks can force laser off (latched until cleared/reset policy is implemented).
void safety_force_laser_off(laser_force_off_reason_t reason);
void safety_force_laser_off_from_isr(laser_force_off_reason_t reason);
uint32_t safety_get_force_off_reasons(void);

// Applies the current decision to the hardware output pin.
void safety_apply_outputs(void);

// Wakes the safety task early (used by ISR).
void safety_notify_from_isr(void);
