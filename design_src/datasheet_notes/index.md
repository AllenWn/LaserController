# Datasheet Notes (Firmware-Oriented)

These notes summarize firmware-relevant details extracted from the PDFs in `design_src/datasheet/`.

Refreshed notes now follow the same structure:

- device role in the system
- firmware-relevant pins
- polarity / open-drain / interface semantics
- board-specific usage
- open items for bring-up

## Main control and sensing devices

- `ESP32-S3-WROOM-1U`: `esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf`
- `LSM6DSO` IMU: `lsm6dso.pdf`
- `ATLS6A214D` laser driver module: `ATLS6A214D.pdf`
- `TEC14M5V3R5AS` TEC controller: `Micro_TEC_Controller_TEC14M5V3R5AS.pdf`
- `DAC80502` dual DAC: `dac80502.pdf`

## Power-path and switching devices

- `TPS22918` load switch: `tps22918.pdf`
- `MPM3530GRF` switched power module: `MPM3530GRF.pdf`
- `MPM3612-33` 3.3 V power module: `MPM3612GLQ-33.pdf`
- `TPS2121` power mux: `tps2121.pdf`
- `STUSB4500` PD sink controller: `stusb4500.pdf`

## Auxiliary-board peripherals

- `MCP23017` GPIO expander: `MCP23017.pdf`
- `TLC59116` LED driver: `tlc59116.pdf`
- `DRV2605` haptic driver: `drv2605.pdf`
