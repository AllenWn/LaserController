# MPM3530GRF (Power Module)

Source: `design_src/datasheet/MPM3530GRF.pdf`

## Firmware-Relevant Pins
- `EN` (enable):
  - No internal pull-up or pull-down; EN must not float.
  - Datasheet lists EN thresholds (typical): rising ~`1.6V`, falling ~`1.3V`, with hysteresis.
- `PG` (power good):
  - Open-drain output; connect pull-up resistor to a pull-up source.
  - Indicates regulation status; datasheet includes VFB-based thresholds and notes protection behaviors that may require recycling EN or VIN to clear.

## Protections (high level)
- OCP, OVP, thermal shutdown are present (see datasheet).

## Firmware-Relevant Notes For This PCB
- ESP32 drives `PWR_*_EN` nets into these modules, and reads `PWR_*_PGOOD` from these modules.

