# STUSB4500

Source PDF: `design_src/datasheet/stusb4500.pdf`

Firmware-relevant summary for the **STUSB4500** USB Type-C / USB-PD sink controller used on the `PD board`.

## 1. What it is

- Standalone **USB Type-C / USB Power Delivery sink controller**
- The MCU does **not** implement the PD protocol directly
- The chip handles:
  - source attach detection
  - CC channel management
  - sink PDO negotiation
  - VBUS sink-path control support

## 2. What this means for firmware

The STUSB4500 is not a generic GPIO peripheral.

Firmware should think of it as:

- a dedicated PD front-end chip
- optionally readable/configurable over I²C
- the component that decides negotiated sink power behavior

So in this project:

- PD protocol is mostly **offloaded to hardware**
- MCU interaction is optional / supervisory, not protocol-critical

## 3. Key pins from datasheet pin list

From the datasheet pin function list:

- `CC1`, `CC2`
  - Type-C configuration channel pins
  - used for attach/orientation/configuration management
- `CC1DB`, `CC2DB`
  - dead-battery related CC pins
- `RESET`
  - **active high reset**
- `SCL`
  - I²C clock, requires external pull-up
- `SDA`
  - I²C data, open-drain, requires external pull-up
- `DISCH`
  - discharge-path control / discharge-related output
- `ATTACH`
  - attachment detection output, open-drain
- `POWER_OK2`, `POWER_OK3`
  - power contract related outputs
- `VBUS_EN_SNK`
  - VBUS sink power path enable, open-drain
- `ALERT`
  - I²C interrupt output, open-drain
- `VREG_1V2`
  - internal 1.2 V regulator output
- `VSYS`
  - power supply from system
- `VREG_2V7`
  - internal 2.7 V regulator output
- `VDD`
  - power supply from USB power line
- `EP`
  - exposed pad to ground

## 4. I²C interface details

The datasheet explicitly calls out the I²C interface pins:

- `SCL`: PC clock, external pull-up required
- `SDA`: PC data, external pull-up required
- `ALERT`: interrupt, external pull-up required
- `ADDR0`, `ADDR1`: I²C device address selection bits

Important warning from datasheet pages viewed:

- `ADDR0` and `ADDR1` must be tied to ground when no MCU is connected

In this project:

- the schematic sets `ADDR0` and `ADDR1` low
- the board note says the device address is set to **`0x28`**

## 5. Logic / polarity behavior relevant here

- `RESET` is **active high**
- `ATTACH` is described as **active low open-drain**
- `POWER_OK2` is described as **high-voltage open-drain**
- `POWER_OK3` is described as **low-voltage open-drain**
- `VBUS_EN_SNK` is an **open-drain** control output

Firmware implication:

- these are **not normal push-pull logic outputs**
- if any of these signals are ever monitored by another MCU input, the pull-up strategy matters

## 6. POWER_OK behavior

The datasheet’s `POWER_OK2 / POWER_OK3` section shows these pins are related to PD contract status and selected PDO behavior.

Important meaning:

- they do **not** simply mean “5V good rail” in the generic regulator sense
- they are PD-contract-related status outputs
- behavior depends on NVM / PDO configuration

In the current board:

- they are only used as local indicators on the PD board
- they are not yet part of the main firmware safety loop

## 7. VBUS power-path control

The datasheet specifically describes:

- `VBUS_EN_SNK`
- `VBUS_VS_DISCH`
- VBUS monitoring / discharge / power-path assertion

In this project:

- the chip drives the external PMOS sink path on the PD board
- the PD board then creates `VSINK`

Firmware implication:

- the main MCU should treat `VSINK` as the result of PD-board hardware behavior
- not as something the ESP32 directly controls pin-by-pin

## 8. Design-specific usage in this project

From the PD board schematic:

- `SDA` / `SCL` are exported off-board as `ST_SDA` / `ST_SCL`
- `CC1` / `CC2` stay on the Type-C front-end side
- `VBUS_EN_SNK` controls the sink-side pass-FET path

So the likely firmware role is:

- optional readback of PD status
- optional parameter/config access over I²C
- not direct real-time PD negotiation

## 9. What firmware should care about

The most important firmware facts are:

- `STUSB4500` is a hardware PD sink controller
- it speaks I²C to the host
- its address is hardware-strapped
- `RESET` is active high
- `ALERT` is available as an interrupt-style signal if routed in the full system
- `VBUS_EN_SNK` / `POWER_OKx` semantics are PD-contract related, not generic GPIO status

## 10. Design-specific conclusions

- This chip is relevant to **system power bring-up and status visibility**
- It is **not** part of the laser safety permit logic unless the team explicitly chooses to use PD status as a gating condition
- For current firmware priorities, it is a **secondary supervisory interface**, not the primary control loop
