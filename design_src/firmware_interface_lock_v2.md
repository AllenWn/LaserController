# Firmware Interface Lock v2

Primary machine-readable files:

- `design_src/cross_board_connector_map.json`
- `design_src/firmware_interface_lock_v2.json`

## What changed from v1

- The lock is now **system-level**, not `MainBoard`-only.
- `GPIO4` and `GPIO5` are no longer treated as generic expansion GPIOs.
  - They are now locked to the cross-board `STUSB4500` I2C path through the `BMS` and `PD` boards.
- `GPIO6` and `GPIO7` are no longer generic spare GPIOs.
  - They are now locked as cross-board lines that land on the `BMS` battery-toggle header.
- The previous temporary assumption that `trigger` was a direct ESP32 GPIO is removed.

## Locked direct MainBoard interfaces

These are safe to use as concrete firmware interfaces now:

- Power sequencing:
  - `PWR_TEC_EN`
  - `PWR_TEC_GOOD`
  - `PWR_LD_EN`
  - `PWR_LD_PGOOD`
- Laser driver:
  - `LD_SBDN`
  - `LD_PCN`
  - `LD_LPGD`
  - `LD_TMO`
  - `LD_LIO`
- TEC controller:
  - `TEC_TEMPGD`
  - `TEC_TMO`
  - `TEC_ITEC`
  - `TEC_VTEC`
- DAC:
  - `DAC_SDA`
  - `DAC_SCL`
- IMU:
  - `IMU_SDI`
  - `IMU_CS`
  - `IMU_SCLK`
  - `IMU_SDO`
  - `IMU_INT2`
- MainBoard haptic:
  - `ERM_SDA`
  - `ERM_SCL`
  - `ERM_EN`

## Locked cross-board interfaces

### PD supervision path

- `GPIO4` -> `ST_SDA` -> `STUSB4500 SDA`
- `GPIO5` -> `ST_SCL` -> `STUSB4500 SCL`

This path is electrically traced through:

- `MainBoard J1`
- `BMS J1`
- `BMS J3`
- `PD J2`

Firmware meaning:

- The ESP32 can optionally supervise or configure the `STUSB4500`
- USB-PD protocol ownership still remains in hardware

### BMS battery-toggle header

- `GPIO6` -> `BMS J2 pin2`
- `GPIO7` -> `BMS J2 pin1`

These are electrically locked, but their final system meaning is still unknown.

## System interfaces now known but not GPIO-locked

### Auxiliary board host interface

The Auxiliary board host-side electrical contract is clear:

- `VDD_3V3`
- `I2C_SCL`
- `I2C_SDA`
- `GPIO_INT_A`
- `GND`

The board behind that interface is also clear:

- `MCP23017` for buttons and interrupt generation
- `TLC59116` for LEDs
- `DRV2605` for haptic output

What is still missing:

- the exact MainBoard landing point or harness definition
- therefore the exact ESP32 pin assignment is not yet locked

## Explicitly removed assumptions

### Trigger direct GPIO

Removed from the lock.

Reason:

- `GPIO4` is now committed to the `PD` board `ST_SDA` path
- The more likely trigger origin is the Auxiliary board buttons through `MCP23017`
- That trigger path is not yet host-landed on the MainBoard schematic set

### Distance sensor direct GPIO

Not locked.

Reason:

- No explicit distance sensor implementation is present in the current four-board schematic set

## State-machine relevant signals that remain locked

### Internal fault class inputs

- `PWR_TEC_GOOD`
- `PWR_LD_PGOOD`
- `LD_LPGD`
- `TEC_TEMPGD`

### Usage fault class inputs

- `IMU` computed orientation result

### Fast laser control

- `LD_SBDN`

### Power sequencing outputs

- `PWR_TEC_EN`
- `PWR_LD_EN`

## Open items before a final v3 lock

- Trace the Auxiliary board host connector to a concrete MainBoard connector or cable definition
- Lock which Auxiliary button is the real laser trigger
- Decide whether `GPIO6/GPIO7` on the `BMS` battery-toggle header belong to firmware at all
- Re-check the system USB differential pair continuity if that path is going to be documented as a locked design interface
