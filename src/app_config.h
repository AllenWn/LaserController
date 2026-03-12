#pragma once

#include "driver/gpio.h"

// Locked interface mapping (pin definitions only).
// NOTE: Some "optional" signals are not present on the current PCB. The baseline
// firmware expects these GPIO macros to be valid numbers (no GPIO_NUM_NC).
// For now they are mapped to spare/test GPIOs and should be revisited in the
// redesign phase.

// Locked control/status GPIOs
#define PWR_TEC_EN_GPIO GPIO_NUM_15
#define PWR_TEC_GOOD_GPIO GPIO_NUM_16
#define PWR_LD_EN_GPIO GPIO_NUM_17
#define PWR_LD_PGOOD_GPIO GPIO_NUM_18

#define LD_TMO_GPIO GPIO_NUM_1
#define LD_LIO_GPIO GPIO_NUM_2
#define LD_SBDN_GPIO GPIO_NUM_13
#define LD_PCN_GPIO GPIO_NUM_21
#define LD_LPGD_GPIO GPIO_NUM_14

#define TEC_TMO_GPIO GPIO_NUM_8
#define TEC_ITEC_GPIO GPIO_NUM_9
#define TEC_VTEC_GPIO GPIO_NUM_10
#define TEC_TEMPGD_GPIO GPIO_NUM_47

// IMU bus/signals (SPI-style pinout per schematic naming)
#define IMU_SDI_GPIO GPIO_NUM_38
#define IMU_CS_GPIO GPIO_NUM_39
#define IMU_SCLK_GPIO GPIO_NUM_40
#define IMU_SDO_GPIO GPIO_NUM_41
#define IMU_INT2_GPIO GPIO_NUM_42

// DAC I2C
#define DAC_SDA_GPIO GPIO_NUM_11
#define DAC_SCL_GPIO GPIO_NUM_12

// USB
#define USB_DP_GPIO GPIO_NUM_20
#define USB_DN_GPIO GPIO_NUM_19

// Boot / strap pins (reserved; do not use as normal runtime control lines)
#define BOOT_STRAP_GPIO GPIO_NUM_0
#define BOOT_OPTION_GPIO GPIO_NUM_46

// Test points
#define TP_IO45_GPIO GPIO_NUM_45
#define TP_IO3_GPIO GPIO_NUM_3

// External expansion / candidate distance interfaces
#define EXP_IO4_GPIO GPIO_NUM_4
#define EXP_IO5_GPIO GPIO_NUM_5
#define EXP_IO6_GPIO GPIO_NUM_6
#define EXP_IO7_GPIO GPIO_NUM_7

// Shared node on current schematic: ERM_TRIG and GN_LD_EN
#define ERM_TRIG_GN_LD_EN_GPIO GPIO_NUM_37
#define ERM_EN_GPIO GPIO_NUM_48
#define ERM_SDA_GPIO GPIO_NUM_35
#define ERM_SCL_GPIO GPIO_NUM_36

// Optional system-level signals (mapped to spare pins for now; update during redesign)
#ifndef LASER_GATE_GPIO
#define LASER_GATE_GPIO GPIO_NUM_45
#endif

#ifndef STATUS_LED_GPIO
#define STATUS_LED_GPIO GPIO_NUM_3
#endif

#ifndef LD_FAULT_N_GPIO
#define LD_FAULT_N_GPIO GPIO_NUM_4
#endif

#ifndef TEC_FAULT_N_GPIO
#define TEC_FAULT_N_GPIO GPIO_NUM_5
#endif

#ifndef ESTOP_N_GPIO
#define ESTOP_N_GPIO GPIO_NUM_6
#endif
