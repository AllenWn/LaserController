# LSM6DSO (IMU)

Source: `design_src/datasheet/lsm6dso.pdf`

## Serial Interface Options
- Registers accessible via I2C or SPI (pins are shared).
- SPI compatibility: SPI modes 0 and 3.

## Mode Selection (`CS`)
- `CS` controls I2C/SPI selection:
  - `CS = 1`: SPI idle / I2C communication enabled
  - `CS = 0`: SPI communication mode / I2C disabled

## I2C Address
- 7-bit slave address pattern: `110101x`
- `SDO/SA0` sets the LSB:
  - `SA0 = 0` (to GND): `1101010b` = `0x6A`
  - `SA0 = 1` (to VDD): `1101011b` = `0x6B`

## Identity Register
- `WHO_AM_I` register: address `0x0F`
- Fixed value: `0x6C`

## Interrupts
- `INT1` and `INT2` pins support interrupt signaling (configurable via registers).

### Interrupt Output Electrical / Polarity (CTRL3_C)
The interrupt pins are configurable; do not assume a fixed polarity/driver.

- Activation level: `H_LACTIVE`
  - `0` (default): interrupt output pins **active high**
  - `1`: interrupt output pins **active low**
- Output driver: `PP_OD`
  - `0` (default): **push-pull** mode
  - `1`: **open-drain** mode

## Pins Used On This PCB (from schematic)
- `CS`, `SCL/SPC`, `SDA/SDI`, `SDO`, `INT2` are routed to the ESP32.
