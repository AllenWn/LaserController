#pragma once

#include "driver/gpio.h"

// NOTE: Pin numbers below are placeholders. Update these to match your schematic.
// Safety defaults are fail-safe: laser output remains OFF unless explicitly permitted.

// Laser gate / enable (TTL) output. Active-high assumed.
#ifndef LASER_GATE_GPIO
#define LASER_GATE_GPIO GPIO_NUM_10
#endif

// Optional status LED output (active-high).
#ifndef STATUS_LED_GPIO
#define STATUS_LED_GPIO GPIO_NUM_48
#endif

// Fault / interlock inputs (active-low assumed). Configure these to match your hardware.
#ifndef LD_FAULT_N_GPIO
#define LD_FAULT_N_GPIO GPIO_NUM_11
#endif

#ifndef TEC_FAULT_N_GPIO
#define TEC_FAULT_N_GPIO GPIO_NUM_12
#endif

#ifndef ESTOP_N_GPIO
#define ESTOP_N_GPIO GPIO_NUM_13
#endif

// Task timing
#ifndef SAFETY_TASK_PERIOD_MS
#define SAFETY_TASK_PERIOD_MS 5
#endif

#ifndef UI_TASK_PERIOD_MS
#define UI_TASK_PERIOD_MS 20
#endif

#ifndef TEC_MONITOR_PERIOD_MS
#define TEC_MONITOR_PERIOD_MS 20
#endif

#ifndef IMU_MONITOR_PERIOD_MS
#define IMU_MONITOR_PERIOD_MS 20
#endif

#ifndef DISTANCE_MONITOR_PERIOD_MS
#define DISTANCE_MONITOR_PERIOD_MS 20
#endif
