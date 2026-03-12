# Firmware Interface Lock v1

Primary machine-readable table:
- `design_src/firmware_interface_lock_v1.json`

## Key Decisions Locked
- Main-board dedicated distance sensor interface: **not present** in current 11-page schematic set.
- Distance path is deferred and mapped to external expansion GPIO candidates (`ESP_GPIO4/5/6/7`) until a concrete distance board/interface is defined.
- Safety-bit source mapping is locked for current design stage:
  - `TEC_READY <- PWR_TEC_GOOD`
  - `TEC_OK <- TEC_TEMPGD`
  - `LD_OK <- PWR_LD_PGOOD + LD_LPGD`
  - `ORIENTATION_OK <- IMU computed status`
  - `DISTANCE_OK <- TBD external distance`
  - `INTERLOCK_OK <- TBD external interlock`

## Important Hardware Constraints
- `GPIO0` and `GPIO46` are boot-strapping pins and are reserved.
- `IO37` node conflict/alias in schematic (`ERM_TRIG` and `GN_LD_EN` on same node) is treated as hardware-coupled behavior unless schematic ECO is made.

