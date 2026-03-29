# ESP32-S3-WROOM-1U

Source PDF: `design_src/datasheet/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf`

Firmware-oriented notes for the **ESP32-S3-WROOM-1U** module used as the main MCU.

## 1. What matters for this project

- `EN` reset / chip-enable behavior
- Strapping pins and boot behavior
- Hardware peripherals:
  - `I²C`
  - `SPI`
  - `UART`
  - `USB`
  - on-chip `ADC`
- GPIO multiplexing flexibility

## 2. `EN` pin semantics

Datasheet pin description:

- `EN high` -> chip on / enabled
- `EN low` -> chip off / reset
- `EN` must not be left floating

Project implication:

- Hardware reset and power sequencing must keep `EN` in a known state
- Firmware does not usually control this pin itself, but system design must respect it

## 3. Strapping pins

The datasheet identifies four strapping GPIOs:

- `GPIO0`
- `GPIO3`
- `GPIO45`
- `GPIO46`

The datasheet also gives their default strap bias:

- `GPIO0` -> weak pull-up
- `GPIO3` -> floating
- `GPIO45` -> weak pull-down
- `GPIO46` -> weak pull-down

Additional meanings:

- `GPIO0` and `GPIO46` participate in boot mode selection
- `GPIO3` is related to early JTAG source selection
- `GPIO45` participates in `VDD_SPI` related strapping behavior

Firmware implication:

- Avoid building safety-critical runtime assumptions on these pins
- If they are used as ordinary GPIOs later, board-level external circuitry must not disturb boot strapping

## 4. Peripheral capability relevant to this project

### I²C

The ESP32-S3 provides hardware I²C controllers.

This matters for:

- `DAC80502`
- `MCP23017`
- `TLC59116`
- `DRV2605`
- optional `STUSB4500`

Project implication:

- Use hardware I²C, not bit-banged GPIO I²C

### SPI

The ESP32-S3 provides general-purpose SPI interfaces.

This matters for:

- `LSM6DSO` IMU in the current design

### ADC

The module exposes on-chip SAR ADC functionality.

This matters for:

- `LD_TMO`
- `LD_LIO`
- `TEC_TMO`
- `TEC_ITEC`
- `TEC_VTEC`

Project implication:

- The ADC exists and can support the design
- But accurate analog supervision still needs attenuation selection, calibration, and real board validation

### USB

The datasheet explicitly supports:

- USB 2.0 full-speed OTG
- USB Serial/JTAG

Pins used in this design:

- `GPIO19` includes `USB_D-`
- `GPIO20` includes `USB_D+`

## 5. Pin capabilities that matter in this project

From the module pin table:

- `GPIO4..GPIO11` include ADC-capable functions on several channels
- `GPIO19` and `GPIO20` carry USB D-/D+
- many GPIOs are highly multiplexed and can be routed to I²C/SPI/UART functions in software

Project implication:

- Most digital peripheral placement is software-routable
- Physical routing and boot constraints still matter more than the abstract peripheral matrix

## 6. Project-specific interpretation

For this firmware project, the ESP32-S3 should be treated as:

- a flexible supervisory MCU
- the owner of safety state, sequencing, and interlock policy
- not the owner of the analog current/temperature loops

## 7. Open items for final bring-up

- Confirm final use of any strap pins in the hardware revision
- Confirm ADC channel attenuation and calibration strategy
- Confirm USB use case:
  - firmware logging only
  - USB Serial/JTAG only
  - or fuller USB function
