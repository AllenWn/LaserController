# TEC14M5V3R5AS

Source PDF: `design_src/datasheet/Micro_TEC_Controller_TEC14M5V3R5AS.pdf`

Firmware-oriented notes for the **TEC14M5V3R5AS** TEC controller used on the `MainBoard`.

## 1. Device role

- Dedicated TEC controller module
- Closes the TEC loop in hardware
- Firmware supervises status and telemetry, but does not implement the control loop itself

## 2. Pins relevant to firmware

From the datasheet pin function table and the board usage:

- `TMO` — analog object-temperature monitor
- `TMS` — analog temperature set input
- `SDNG` — shutdown / multi-function control-related pin
- `TEMPGD` — digital temperature-good indication
- `VTEC` — analog TEC voltage indication
- `ITEC` — analog TEC current indication
- `ILIM`, `VLIM` — analog limit-setting inputs

## 3. Digital status output

### `TEMPGD`

Datasheet meaning:

- temperature-good indication output

Project-level interpretation:

- `HIGH` -> TEC temperature condition acceptable
- `LOW` -> not within acceptable condition

This is the main digital TEC health signal used by the firmware design.

## 4. Analog monitor outputs

### `TMO`

Datasheet meaning:

- actual object temperature indication
- about `0.1 V` to `2.5 V` across the default temperature network range

Firmware implication:

- This is an ADC signal, not a digital fault line
- Final threshold use requires board-level calibration and the chosen thermal operating window

### `ITEC`

Datasheet relationship:

- `ITEC = (V_ITEC - 1.25 V) / 0.285`

Firmware implication:

- TEC current can be derived from ADC voltage
- This should be treated as monitored telemetry unless a validated threshold policy is defined

### `VTEC`

Datasheet meaning:

- analog TEC voltage indication

The datasheet notes limit relations through `VLIM`, but final use in firmware still depends on board-specific scaling and validation.

## 5. Control-related pins

### `TMS`

Datasheet meaning:

- analog temperature set input

Project implication:

- This is a command/setpoint input
- It is not a direct digital safety indicator

### `SDNG`

Datasheet meaning:

- shutdown-related multifunction pin

Project implication:

- The board currently uses `TEMPGD` as the primary firmware-facing health signal
- `SDNG` still matters for understanding the controller behavior, but it is not the main MCU supervision path in the current mainboard design

## 6. Project-specific interpretation

The TEC controller should be modeled in firmware as:

- one digital health output: `TEMPGD`
- several ADC telemetry outputs: `TMO`, `ITEC`, `VTEC`
- one analog command path: `TMS`

## 7. Open items for final bring-up

- Confirm actual `TEMPGD` polarity on hardware
- Validate `TMO`, `ITEC`, and `VTEC` scaling with real measurements
- Decide which analog TEC signals become hard safety thresholds versus logged telemetry only
