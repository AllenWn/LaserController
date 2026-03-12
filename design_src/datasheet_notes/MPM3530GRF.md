# MPM3530GRF (Power Module)

Source: `design_src/datasheet/MPM3530GRF.pdf`

## Firmware-Relevant Pins
- `EN` (enable):
  - No internal pull-up or pull-down; EN must not float.
  - Datasheet lists EN thresholds (typical): rising ~`1.6V`, falling ~`1.3V`, with hysteresis.
- `PG` (power good):
  - Open-drain output; connect pull-up resistor to a pull-up source.
  - Datasheet behavior summary (from the "Power Good (PG)" description):
    - During startup, `PG` is pulled to GND (open-drain MOSFET on).
    - When regulation is achieved (datasheet uses `VFB` reaching ~90% of `VREF`), the MOSFET turns off and `PG` is pulled high by the external pull-up.
    - Therefore, with an external pull-up, `PG = HIGH` indicates "power good", `PG = LOW` indicates "not good" (startup/fault/disabled).

## Protections (high level)
- OCP, OVP, thermal shutdown are present (see datasheet).

## Firmware-Relevant Notes For This PCB
- ESP32 drives `PWR_*_EN` nets into these modules, and reads `PWR_*_PGOOD` from these modules.
