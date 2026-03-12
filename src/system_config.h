#pragma once

// System-level configuration (timing, safety policy).
// Keep pin mapping in app_config.h.

// Task periods
#ifndef SUPERVISOR_TASK_PERIOD_MS
#define SUPERVISOR_TASK_PERIOD_MS 5
#endif

#ifndef STATUS_LOG_PERIOD_MS
#define STATUS_LOG_PERIOD_MS 500
#endif

// Module staleness timeouts (if a module stops updating, treat as NOT OK).
#ifndef PWR_STATUS_TIMEOUT_MS
#define PWR_STATUS_TIMEOUT_MS 200
#endif

#ifndef LD_STATUS_TIMEOUT_MS
#define LD_STATUS_TIMEOUT_MS 200
#endif

#ifndef TEC_STATUS_TIMEOUT_MS
#define TEC_STATUS_TIMEOUT_MS 200
#endif

#ifndef IMU_STATUS_TIMEOUT_MS
#define IMU_STATUS_TIMEOUT_MS 100
#endif

