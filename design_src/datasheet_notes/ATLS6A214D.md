# ATLS6A214D (6A Laser Driver Module)

Source: `design_src/datasheet/ATLS6A214D.pdf`

## Control States
- The driver supports Operation, Standby, and Shutdown states.

## Key Control / Status Pins
- `TMO` (analog output): controller temperature monitor.
  - Datasheet provides an approximation (recommended linear equation in typical 40°C–100°C range):
    - `T(°C) ≈ 192.5576 − 90.1040 × V_TMO`
    - `V_TMO ≈ 2.1365 − 0.0111 × T(°C)`
  - This is an **analog voltage**, not a digital “good/bad” signal.
- `SBDN` (input): Standby / Shutdown control via analog/digital level (3-level behavior).
  - `0V to 0.4V`: Shutdown mode
  - `2.1V to 2.4V`: Standby mode
  - `> 2.6V`: Operation (release from standby/shutdown)
- `PCN` (digital input): selects between two current set ports (LISL vs LISH).
  - Low selects LISL; high selects LISH (see datasheet for voltage ranges).
  - Rise/fall time of output when switching PCN: ~`28 us` (typical noted).
- `LPGD` (digital output): loop-good indication.
  - Datasheet describes an internal pull-up (to internal 4.5V rail) and an open-drain pull-down.
  - Voltage semantics called out:
    - `LPGD > 2V`: loop working properly ("good")
    - `LPGD < 0.3V`: loop not working properly / standby / shutdown ("not good")
- `LIO` (analog output): output current monitor.
  - Datasheet description:
    - `V_LIO = (I_OUT / 6A) × 2.5V`
    - Therefore `I_OUT = (V_LIO / 2.5V) × 6A`
  - Example given: `I_OUT = 0A` ⇒ `V_LIO = 0V`; `I_OUT = 6A` ⇒ `V_LIO = 2.5V`.
  - This is an **analog voltage**, not a digital “good/bad” signal.

## Timing (typical values called out in datasheet)
- Start-up time after releasing `SBDN` above `2.6V`: ~`20 ms`
- Shutdown time upon pulling `SBDN` down: ~`20 us`

## Firmware-Relevant Notes For This PCB
- `SBDN` is driven by an ESP32 GPIO (3-level control may require GPIO modes or external network as designed).
- `PCN` is driven by an ESP32 GPIO (select low/high current setpoint path).
- `LPGD` is read by the ESP32 as a status input.
- `TMO` and `LIO` appear on ESP32 pins on this PCB; treat as **ADC inputs** if used (not GPIO digital inputs).
