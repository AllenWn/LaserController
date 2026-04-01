# Design Source Index

## 1. Current `design_src` Structure

The current design assets are organized into four main groups:

- `design_src/schematic/`
  Raw schematic PDFs for each hardware board in the system
- `design_src/mainboard/`, `design_src/auxiliary/`, `design_src/pd/`, `design_src/bms/`
  Board-scoped extracted assets such as rendered page images and semantic JSON files
- `design_src/datasheet/` and `design_src/datasheet_notes/`
  Component datasheets and firmware-oriented notes

## 2. Board-Level Schematic Assets

### 2.1 MainBoard

File:

- `design_src/schematic/MainBoard_Schematic.pdf`

Role in project:

- Primary control board
- Contains the `ESP32-S3-WROOM-1U`
- Hosts the direct MCU interfaces used by the current firmware design
- Includes the LD, TEC, DAC, IMU, ERM-related, USB, and B2B connector interfaces

Current extraction status:

- Already analyzed
- Semantic outputs already exist as:
  - `design_src/mainboard/semantic/schematic_page_01_semantic.json`
  - through
  - `design_src/mainboard/semantic/schematic_page_11_semantic.json`
- Rendered page images already exist as:
  - `design_src/mainboard/pages/page-01.png`
  - through
  - `design_src/mainboard/pages/page-11.png`
- Current firmware design and interface lock are based mainly on this board

### 2.2 PD Board

File:

- `design_src/schematic/USB_PD_Schematic.pdf`

Preliminary role assessment:

- USB Type-C / Power Delivery front-end board
- Likely handles external power entry, CC negotiation, protection, and upstream power conditioning before power reaches the rest of the system

Evidence currently visible from PDF text extraction:

- `stusb4500qtr`
- `TPD6S300ARUKR`
- USB Type-C receptacle
- TVS / protection parts
- LDO device

Expected design relevance:

- Clarifies how system input power is negotiated and protected
- May explain what the main board actually receives from the external power subsystem
- May define additional sideband or connector relationships relevant to system-level bring-up

Current extraction status:

- Raw PDF present
- Semantic outputs now exist as:
  - `design_src/pd/semantic/schematic_page_01_semantic.json`
  - through
  - `design_src/pd/semantic/schematic_page_03_semantic.json`
- Board-level summary exists as:
  - `design_src/pd/pd_board_summary.json`

### 2.3 Auxiliary Board

File:

- `design_src/schematic/Auxiliary_Schematic.pdf`

Preliminary role assessment:

- External auxiliary / UI / sensor / feedback board
- Likely the board reached through the B2B expansion lines from the main board

Evidence currently visible from PDF text extraction:

- `MCP23017T-E/ML` GPIO expander
- `TLC59116IRHBR` LED driver
- `DRV2605LDGSR` haptic driver
- tactile switch
- magnetic connector
- shared I2C references across connectors / LEDs / LRA driver / GPIO expander

Expected design relevance:

- Very likely defines the actual use of some B2B-expanded GPIO signals
- Likely contains trigger / buttons / indicators / haptic / user-interface related logic
- May also define how some external signals are aggregated through an expander rather than directly into the ESP32

Current extraction status:

- Raw PDF present
- Semantic outputs now exist as:
  - `design_src/auxiliary/semantic/schematic_page_01_semantic.json`
  - through
  - `design_src/auxiliary/semantic/schematic_page_05_semantic.json`
- Board-level summary exists as:
  - `design_src/auxiliary/auxiliary_board_summary.json`

### 2.4 BMS Board

File:

- `design_src/schematic/BMS_Schematic.pdf`

Preliminary role assessment:

- Expected to be a battery-management or system power selection board
- The exact role is not yet fully confirmed from current lightweight text extraction

Evidence currently visible from PDF text extraction:

- `TPS2121RUXR` power mux
- board-to-board connector references
- magnetic connector references

Important note:

- Current text extraction did **not** yet reveal obvious battery charger / gauge / protection IC names
- So the board name suggests BMS, but the exact implemented function still needs full schematic inspection

Expected design relevance:

- May define battery-side or alternate power-path selection
- May determine whether the MCU ever sees battery status directly, or only indirectly through other boards
- May also explain how external connectors and power rails are routed between boards

Current extraction status:

- Raw PDF present
- Semantic outputs now exist as:
  - `design_src/bms/semantic/schematic_page_01_semantic.json`
  - through
  - `design_src/bms/semantic/schematic_page_02_semantic.json`
- Board-level summary exists as:
  - `design_src/bms/bms_board_summary.json`

## 3. Coverage Status Summary

### Already Covered

- MainBoard schematic semantic extraction
- PD board schematic semantic extraction
- Auxiliary board schematic semantic extraction
- BMS board schematic semantic extraction
- MainBoard-oriented firmware interface map
- MainBoard-oriented firmware interface lock
- MainBoard-based firmware architecture and current implementation

### Not Yet Covered

- Cross-board connector mapping between MainBoard and the other three boards
- Updated system-level interface lock after those three boards are analyzed

## 4. Why These Three New Boards Matter

The original firmware design was constrained by what was visible on the main board alone.

That created several unresolved areas:

- trigger final source not fully confirmed
- distance path deferred
- external B2B-expanded GPIO purpose only partially known
- PD/BMS side behavior not fully understood

These new schematic files are important because they may resolve exactly those unknowns.

In particular:

- `Auxiliary` is likely to explain the UI / trigger / LED / haptic / expansion side
- `PD` is likely to explain the external power entry and protection side
- `BMS` is likely to explain battery / alternate power path / system supply routing side

## 5. Recommended Next Extraction Outputs

For consistency with the existing main-board workflow, the next useful outputs should be:

- one semantic JSON set per new board
- one board-level summary file per new board
- one cross-board connector map
- one updated system-level firmware interface lock after cross-board analysis

Recommended output layout:

- `design_src/mainboard/semantic/*.json`
- `design_src/auxiliary/semantic/*.json`
- `design_src/pd/semantic/*.json`
- `design_src/bms/semantic/*.json`
- `design_src/<board>/pages/*.png`
- `design_src/board_role_summary.json`
- `design_src/cross_board_connector_map.json`
- `design_src/firmware_interface_lock_v2.md`
- `design_src/firmware_interface_lock_v2.json`

## 6. Recommended Analysis Order

The recommended order for the next stage is:

1. Analyze `Auxiliary` first
   Because it is the most likely source of trigger / UI / B2B-expanded GPIO meaning
2. Build a cross-board connector map
3. Update the firmware interface lock and system design documents

## 7. Current Working Conclusion

At this point, the firmware design is still correctly based on the MainBoard schematic, but it is not yet complete at the whole-system level.

The addition of:

- `PD_Schematic`
- `BMS_Schematic`
- `Auxiliary_Schematic`

means the project has now moved from:

- **main-board-centric firmware design**

to:

- **multi-board system-level firmware design**

and the next design iteration should reflect that.
