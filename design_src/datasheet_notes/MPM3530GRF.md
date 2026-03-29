# MPM3530GRF

Source PDF: `design_src/datasheet/MPM3530GRF.pdf`

Firmware-oriented notes for the **MPM3530GRF** buck power module used on the `MainBoard`.

## 1. Device role

- Integrated buck power module
- Used to generate controlled supply rails for major subsystems
- Exposes a control input and a health output relevant to firmware

In this project, the firmware interacts with this device only through the board-level enable and power-good nets.

## 2. Relevant pins

From the datasheet pin table:

- `EN` — enable input
- `PG` — power-good output

## 3. `EN` semantics

Datasheet meaning:

- Pull `EN` below threshold -> shut down the regulator
- No internal pull-up or pull-down
- `EN` must not be left floating

Datasheet thresholds called out:

- rising threshold around `1.6 V`
- falling threshold around `1.3 V`

Project implication:

- Firmware-driven enable nets must always drive a defined logic state
- This is a real power-stage control line, not just a logical permission bit

## 4. `PG` semantics

Datasheet meaning:

- `PG` is a power-good indication
- Requires an external pull-up

Practical firmware interpretation:

- `HIGH` -> output rail is in regulation
- `LOW` -> startup, disabled, or fault / not good

This matches how the current firmware uses `PWR_*_PGOOD` style signals.

## 5. Firmware relevance

For the firmware, the important distinction is:

- `EN` controls whether the rail is requested on
- `PG` reports whether the rail is actually healthy

These two signals must not be collapsed into one logical concept.

## 6. Project-specific interpretation

The MPM3530GRF belongs to the **power-sequencing chain**, not directly to the laser emission gate.

Firmware should use it for:

- staged power-up
- staged power-down
- fault detection
- re-entry protection after internal faults

## 7. Open items for final bring-up

- Confirm actual `PG` polarity on the populated board
- Confirm whether both LD and TEC switched rails use the same regulator family configuration
- Validate `PG` timing against the power-up timeout values in the firmware
