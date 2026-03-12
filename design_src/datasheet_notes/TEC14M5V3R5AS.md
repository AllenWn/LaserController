# TEC14M5V3R5AS (Micro TEC Controller)

Source: `design_src/datasheet/Micro_TEC_Controller_TEC14M5V3R5AS.pdf`

## Key Pins (firmware-relevant)
- `TEMPGD` (digital output): temperature-good indication.
- `TMO` (analog output): actual object temperature indication.
  - Datasheet pin table states: **`0.1V` to `2.5V` indicates the default temperature network range**.
- `VTEC` (analog output): TEC voltage indication.
- `ITEC` (analog output): TEC current indication.
  - Datasheet pin table states: `ITEC = (VITEC − 1.25) / 0.285`, where `VITEC` is the voltage on the `ITEC` pin and `ITEC` is defined as current flowing into `TEC+` and out of `TEC−`.
- `TECP` / `TECN`: TEC terminals (power).
- `RTH`: inverting input to error amplifier.

## Firmware-Relevant Notes For This PCB
- `TEMPGD` is routed to the ESP32 as a GPIO input.
- `TMO`, `VTEC`, `ITEC` appear on ESP32 pins on this PCB. These are **analog monitor outputs**; do not treat them as “high/low = good/bad” unless the PCB has extra comparators/level-shifters that convert them to digital.
  - If you want to use them in firmware, they should be routed into an ESP32 ADC-capable GPIO (and the analog front-end / scaling must be confirmed).

## Pin Semantics Summary (what HIGH/LOW means)
- `TEMPGD` (digital):
  - Datasheet labels it “Temperature good indication” but **does not clearly state polarity** in the extracted text.
  - Current project assumption (based on board indicator usage): **`TEMPGD = HIGH` ⇒ “good”** (must verify on hardware).
- `TMO`, `VTEC`, `ITEC`:
  - These are **analog voltages**, so “HIGH/LOW = good/bad” is not a defined concept; instead firmware must compare against thresholds after ADC conversion (thresholds are system-level requirements, not fixed by the chip).

## Other Digital/Special Pins (not currently used on this PCB)
- `SDNG` (pin 9, “Shutdown/Temperature good”, analog I/O):
  - Datasheet pin table: **pulling `SDNG` low shuts down the device**.
  - If left unconnected, it outputs a temperature-related indication (voltage levels described in the datasheet pin table).
