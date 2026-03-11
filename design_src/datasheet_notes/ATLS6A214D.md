# ATLS6A214D (6A Laser Driver Module)

Source: `design_src/datasheet/ATLS6A214D.pdf`

## Control States
- The driver supports Operation, Standby, and Shutdown states.

## Key Control / Status Pins
- `SBDN` (input): Standby / Shutdown control via analog/digital level (3-level behavior).
  - `0V to 0.4V`: Shutdown mode
  - `2.1V to 2.4V`: Standby mode
  - `> 2.6V`: Operation (release from standby/shutdown)
- `PCN` (digital input): selects between two current set ports (LISL vs LISH).
  - Low selects LISL; high selects LISH (see datasheet for voltage ranges).
  - Rise/fall time of output when switching PCN: ~`28 us` (typical noted).
- `LPGD` (digital output): loop-good indication.
  - Datasheet notes low level (for example `< 0.3V`) indicates not working properly; high (for example `> 2V`) indicates loop OK.

## Timing (typical values called out in datasheet)
- Start-up time after releasing `SBDN` above `2.6V`: ~`20 ms`
- Shutdown time upon pulling `SBDN` down: ~`20 us`

## Firmware-Relevant Notes For This PCB
- `SBDN` is driven by an ESP32 GPIO (3-level control may require GPIO modes or external network as designed).
- `PCN` is driven by an ESP32 GPIO (select low/high current setpoint path).
- `LPGD` is read by the ESP32 as a status input.

