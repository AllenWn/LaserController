# LSM6DSO

Source PDF: `design_src/datasheet/lsm6dso.pdf`

Firmware-oriented notes for the **LSM6DSO** IMU used on the `MainBoard`.

## 1. Device role

- 6-axis inertial sensor
- 3-axis accelerometer + 3-axis gyroscope
- Supports:
  - `SPI`
  - `I²C`
  - `I3C` related modes in the broader family documentation

In this project it is used as a safety-relevant orientation sensor.

## 2. Interface selection

The part supports both SPI and I²C style host interfaces.

Project implication:

- The mainboard routes it in an SPI-style connection
- Firmware should treat it as an SPI peripheral in the current design

## 3. Identity and addressing

Datasheet register:

- `WHO_AM_I = 0x6C`

If operated in I²C mode, the typical addresses are:

- `0x6A`
- `0x6B`

depending on `SDO/SA0`.

Firmware implication:

- Even in SPI mode, `WHO_AM_I = 0x6C` is the basic bring-up check

## 4. Interrupt outputs

The device provides:

- `INT1`
- `INT2`

Datasheet capability:

- interrupt polarity can be configured
- outputs can be push-pull or open-drain

Project implication:

- The final interrupt polarity cannot be assumed until firmware configures the part
- The current project still supports pure polling, with interrupt use as an optional later enhancement

## 5. Pins relevant to this board

From the mainboard schematic:

- `CS`
- `SCL/SPC`
- `SDA/SDI`
- `SDO/SA0`
- `INT2`

`INT1` is not currently routed in the mainboard design.

## 6. What firmware must do

At minimum:

- initialize the SPI bus
- verify device identity with `WHO_AM_I`
- configure ODR / scale / filtering as needed
- read acceleration data
- convert raw measurements into orientation logic

## 7. Safety meaning in this project

The IMU does not produce a direct “safe / unsafe” hardware output by itself.

Firmware responsibility is to:

- read the data
- compute tilt / pose
- compare against the allowed operating range
- feed that result into the safety state machine

## 8. Project-specific interpretation

The LSM6DSO is:

- a safety-relevant sensing device
- communication-based, not GPIO-based
- slower than a hardwired digital fault line, but richer in information

## 9. Open items for final bring-up

- Confirm final ODR and filter settings
- Confirm the exact orientation computation convention used by the mechanical team
- Decide whether to remain polling-only or later use `INT2` for data-ready or motion event assistance
