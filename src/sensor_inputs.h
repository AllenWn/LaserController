#pragma once

#include <stdbool.h>

typedef struct
{
  bool tec_ready;
  bool tec_thermal_ok;
} tec_status_t;

typedef struct
{
  bool orientation_ok;
} imu_status_t;

typedef struct
{
  bool distance_ok;
} distance_status_t;

// Hardware integration hooks.
// Return true when a fresh valid sample is available.
// Return false on timeout/comm error/not implemented.
bool tec_read_status(tec_status_t *out);
bool imu_read_status(imu_status_t *out);
bool distance_read_status(distance_status_t *out);

