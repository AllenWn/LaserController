#include "sensor_inputs.h"

bool __attribute__((weak)) tec_read_status(tec_status_t *out)
{
  (void)out;
  return false;
}

bool __attribute__((weak)) imu_read_status(imu_status_t *out)
{
  (void)out;
  return false;
}

bool __attribute__((weak)) distance_read_status(distance_status_t *out)
{
  (void)out;
  return false;
}

