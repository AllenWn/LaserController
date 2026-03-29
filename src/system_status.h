#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"

// Safety condition bits computed by supervisor from module status snapshots.
typedef enum
{
  SAFETY_BIT_PWR_TEC_READY = (1u << 0),
  SAFETY_BIT_PWR_LD_READY = (1u << 1),
  SAFETY_BIT_TEC_OK = (1u << 2),
  SAFETY_BIT_LD_OK = (1u << 3),
  SAFETY_BIT_IMU_ORIENT_OK = (1u << 4),
} safety_bit_t;

typedef struct
{
  bool tec_ready;
  bool ld_ready;
  TickType_t updated_at;
} pwr_status_t;

typedef struct
{
  bool ok;
  bool pwr_good;
  bool loop_good;
  TickType_t updated_at;
} ld_digital_status_t;

typedef struct
{
  bool ok;
  bool pwr_good;
  bool temp_good;
  TickType_t updated_at;
} tec_digital_status_t;

typedef struct
{
  // Analog-derived OK verdict from LD monitor (if enabled).
  bool ok;
  bool temp_valid;
  float temp_c;
  bool current_valid;
  float current_a;
  TickType_t updated_at;
} ld_analog_status_t;

typedef struct
{
  bool ok;
  bool temp_valid;
  float temp_c;
  bool itec_valid;
  float itec_a;
  bool vtec_valid;
  float vtec_v;
  TickType_t updated_at;
} tec_analog_status_t;

typedef struct
{
  bool valid;
  bool orientation_ok;
  float angle_from_down_deg;
  TickType_t updated_at;
} imu_status_t;

typedef struct
{
  bool device_present;
  bool i2c_ok;
  bool attached_known;
  bool attached;
  bool contract_valid_known;
  bool contract_valid;
  bool fault_known;
  bool fault;
  TickType_t updated_at;
} pd_status_t;

typedef struct
{
  // Module snapshots (latest values written by monitor tasks).
  pwr_status_t pwr;
  ld_digital_status_t ld_dig;
  tec_digital_status_t tec_dig;
  ld_analog_status_t ld_ana;
  tec_analog_status_t tec_ana;
  imu_status_t imu;
  pd_status_t pd;

  // Latched system faults (set by supervisor or ISR later).
  uint32_t fault_latch;
} system_status_snapshot_t;

void system_status_init(void);

// Update APIs (called by monitor tasks)
void system_status_update_pwr(bool tec_ready, bool ld_ready, TickType_t now);
void system_status_update_ld_digital(const ld_digital_status_t *st, TickType_t now);
void system_status_update_tec_digital(const tec_digital_status_t *st, TickType_t now);
void system_status_update_ld_analog(const ld_analog_status_t *st, TickType_t now);
void system_status_update_tec_analog(const tec_analog_status_t *st, TickType_t now);
void system_status_update_imu(const imu_status_t *st, TickType_t now);
void system_status_update_pd(const pd_status_t *st, TickType_t now);

// Fault latch helpers
void system_status_fault_latch(uint32_t fault_mask);
void system_status_fault_clear(uint32_t fault_mask);

// Snapshot read (called by supervisor)
system_status_snapshot_t system_status_get_snapshot(void);
