# ESP32-S3-WROOM-1 / ESP32-S3-WROOM-1U (Module)

Source: `design_src/datasheet/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf`

## Boot / Strapping
- Strapping pins are latched at reset and then available during runtime.
- `GPIO0` and `GPIO46` control boot mode after reset is released.
- Internal default pulls (weak):
  - `GPIO0`: weak pull-up
  - `GPIO46`: weak pull-down
- Signals on strapping pins must meet setup/hold time around reset.

## Reset / Enable (`EN`)
- `EN` is used for power-up and reset behavior.
- After power rails stabilize, `EN` is pulled high to activate the chip.

## Interfaces / Peripherals (high level)
- Full-speed USB 2.0 OTG (integrated transceiver).
- UART, I2C, SPI controllers available.
- I2C: ESP32-S3 has two I2C controller peripherals (can be configured master/slave).
- SPI: multiple SPI controllers exist; some reserved for flash/PSRAM, others general-purpose.

## Practical Firmware Constraints For This PCB
- Avoid using `GPIO0` / `GPIO46` for safety-critical signals due to boot strapping role.
- When assigning GPIOs for buses (I2C/SPI), ensure chosen pins do not interfere with boot strapping.

