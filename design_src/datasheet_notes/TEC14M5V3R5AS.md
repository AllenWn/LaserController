# TEC14M5V3R5AS (Micro TEC Controller)

Source: `design_src/datasheet/Micro_TEC_Controller_TEC14M5V3R5AS.pdf`

## Key Pins (firmware-relevant)
- `TEMPGD` (digital): temperature-good indication output.
- `TMO` (analog): actual object temperature indication (datasheet mentions 0.1V to 2.5V for default temperature network range).
- `VTEC` (analog): TEC voltage indication.
- `ITEC` (output): TEC current related output; datasheet provides formula of form `ITEC = (VITEC - 1.25) / 0.285`.
- `TECP` / `TECN`: TEC terminals (power).
- `RTH`: inverting input to error amplifier.

## Firmware-Relevant Notes For This PCB
- `TEMPGD` is routed to the ESP32 as a GPIO input.
- `TMO`, `VTEC`, `ITEC` appear on ESP32 pins on this PCB; confirm whether they are treated as digital-level signals or require analog measurement path in the full system design.

