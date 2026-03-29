# ATLS6A214D

Source PDF: `design_src/datasheet/ATLS6A214D.pdf`

Firmware-oriented notes for the **ATLS6A214D** laser driver module used on the `MainBoard`.

## 1. Device role

- High-voltage constant-current laser diode driver
- Input supply range called out in datasheet: `5 V` to `14 V`
- Output current range: up to `6 A`
- Three operating states:
  - `Shutdown`
  - `Standby`
  - `Operation`

In this project, the analog current loop is implemented inside this module. Firmware only supervises it and controls its operating state.

## 2. Pins that matter to firmware

From the datasheet pin table and the mainboard design:

- `TMO` — analog temperature monitor output
- `LPGD` — digital loop-good status output
- `SBDN` — 3-level state control input
- `PCN` — digital selection input for the two current-set ports
- `2.5VR` — 2.5 V reference output
- `LISL` — analog current-set input used when `PCN` selects low bank
- `LISH` — analog current-set input used when `PCN` selects high bank
- `LIO` — analog laser current monitor output

## 3. Confirmed control semantics

### `SBDN`

The datasheet describes three ranges:

- `0 V` to `0.4 V` -> `Shutdown`
- `2.1 V` to `2.4 V` -> `Standby`
- `2.6 V` to `14 V` -> `Operation`

Firmware implication:

- `SBDN` is not a simple enable pin
- The board can implement three states by driving:
  - `LOW`
  - `HIGH-Z`
  - `HIGH`

This is exactly how the current firmware design uses it.

### `PCN`

The datasheet states:

- `PCN low` -> output current follows `LISL`
- `PCN high` -> output current follows `LISH`

Additional datasheet details:

- Internal pull-up to an internal `4.5 V` rail through about `1 MΩ`
- Output current transition time when switching `PCN` is on the order of `28 µs`

Board-level implication in this project:

- `LISH` is tied to `GND`
- `LISL` is driven from `DAC_OUTA`

So the current board revision does not really use `PCN` as a dynamic two-bank current selector. Firmware can treat it as a board-configured mode line.

## 4. Digital status output

### `LPGD`

Datasheet meaning:

- Indicates whether the current loop is regulating correctly
- Internally pulled up high through a resistor when loop is good
- Pulled low by an open-drain transistor when loop is not good

Practical firmware interpretation:

- `HIGH` -> loop good / output current matches commanded setpoint
- `LOW` -> loop not good, open-circuit, overload, disabled, or protected state

This is a valid digital health signal for the safety logic.

## 5. Analog monitor outputs

### `TMO`

Datasheet meaning:

- Analog temperature indication output
- Intended to be read by ADC if temperature supervision is needed

The datasheet provides a nonlinear relationship and also a linear approximation:

- `T(°C) ≈ 192.5576 - 90.1040 × V_TMO`

Use this only as an initial engineering approximation. Final firmware thresholds should be based on hardware validation.

### `LIO`

Datasheet relationship:

- `V_LIO = (I_OUT / 6 A) × 2.5 V`

Equivalent form:

- `I_OUT = V_LIO / 2.5 × 6 A`

Firmware implication:

- `LIO` is true analog telemetry
- It is useful for ADC-based monitoring, but it is not a direct digital fault signal

## 6. Reference and setpoint pins

### `2.5VR`

Datasheet meaning:

- Precision `2.5 V` reference output
- Can source and sink current
- Still active in `Standby`

Project implication:

- This reference is intended to support analog setpoint generation
- It explains why the LD sheet uses a `0 V` to `2.5 V` style control range

### `LISL` and `LISH`

Datasheet meaning:

- `0 V` to `2.5 V` on either input corresponds linearly to `0 A` to `6 A` output current

Firmware implication:

- If DAC output is used to drive one of these pins, the DAC code must map into the required output current range
- The exact current command is a system-level design decision, not a safety-state-machine decision

## 7. Timing values relevant to firmware

Datasheet values explicitly called out:

- Start-up time after releasing `SBDN` above `2.6 V`: about `20 ms`
- Shutdown time after pulling `SBDN` low: about `20 µs`

Design implication:

- `SBDN` is the fast emission control path
- Power rail enables should be treated separately from the actual laser emission gate

## 8. Project-specific interpretation

For this firmware, the ATLS6A214D should be modeled as:

- `SBDN` = primary fast operating-state control
- `LPGD` = primary digital loop-health input
- `TMO` and `LIO` = optional ADC telemetry inputs
- `PCN` = board-dependent mode/select line, not the main safety gate

## 9. Open items for final bring-up

- Confirm actual `LPGD` polarity on real hardware
- Validate the `TMO` transfer function against measured temperature
- Confirm whether `PCN` is intended to stay fixed in this board revision or may be repurposed later
