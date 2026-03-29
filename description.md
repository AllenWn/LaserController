# NIR Laser Controller Firmware Design Description

## 1. Document Purpose

This document describes the firmware design for the handheld near-infrared surgical laser controller at the system-logic level.

The goal of this document is not to describe source code structure or implementation details. The goal is to define the intended firmware behavior clearly enough that the same firmware architecture and control logic could be reproduced from this document alone.

This document is therefore used to validate:

- the control philosophy of the system
- the safety model
- the state machine behavior
- the relationship between input signals, safety decisions, and outputs
- the intended behavior during startup, normal operation, and fault conditions

If later code differs from this document, this document should be treated as the design reference and the code should be corrected to match the design.

## 2. System Mission

The firmware runs on an `ESP32-S3-WROOM-1U` and acts as the supervisory controller of the laser device.

The MCU does **not** directly implement:

- the laser current regulation loop
- the TEC temperature regulation loop
- the USB-C Power Delivery negotiation loop

Those functions are handled by dedicated hardware.

The MCU **does** implement:

- system startup supervision
- safety monitoring
- state machine control
- laser emission permission control
- fault classification and recovery policy
- output coordination for LD, TEC, and DAC setpoint gating

The firmware is therefore a real-time supervisory safety controller.

## 3. Top-Level Control Philosophy

The entire system is centered around one main question:

**Is laser emission allowed right now?**

To answer that question, the firmware combines:

- hardware power-good signals
- hardware loop-good / temp-good signals
- IMU orientation safety
- operator trigger input
- internal fault history and latch state

The result of that evaluation controls:

- whether the system may enter firing mode
- whether the LD output control line is allowed to command operation
- whether the system remains in a powered ready state
- whether the system must transition into a fault state

The firmware therefore has two related but distinct responsibilities:

1. keep the hardware powered and supervised safely
2. permit or deny laser emission safely

These two responsibilities are intentionally separated because "device is powered" and "laser may emit" are not the same condition.

## 4. Safety Model

The safety model is layered.

### 4.1 Hardware-Level Safety

The fastest and most local protection is implemented in hardware:

- the LD driver regulates laser current itself
- the TEC controller regulates temperature itself
- power controllers generate `PGOOD` signals
- driver/controller hardware generates "good" or monitor outputs

The firmware does not replace those protections. It supervises them.

### 4.2 Firmware Supervisory Safety

The firmware continuously checks whether the whole system remains within the approved operating envelope.

This includes:

- whether the TEC power domain is healthy
- whether the LD power domain is healthy
- whether the TEC controller reports healthy thermal state
- whether the LD driver reports healthy loop state
- whether the device orientation is acceptable for use
- whether the operator is requesting emission

### 4.3 Fail-Safe Policy

Whenever there is uncertainty, the system prefers the safer action.

Examples:

- if fresh safety data is unavailable, treat that condition as not safe
- if a required monitor task stops updating, emission is denied
- if an internal electrical fault occurs, outputs fall back to safe state
- if the operator behavior is unsafe, emission is denied even if power remains present

## 5. Functional Partitioning

The firmware is divided conceptually into five layers.

### 5.1 Hardware Interface Layer

This layer defines the MCU-facing signals and communication channels.

It includes:

- digital GPIO inputs
- digital GPIO outputs
- SPI communication with the IMU
- I2C communication with the DAC
- ADC-based analog acquisition paths that are planned but not yet fully defined in design

### 5.2 Monitor Layer

This layer samples subsystems and converts raw signals into subsystem-level health results.

Examples:

- power monitor evaluates `PWR_TEC_GOOD` and `PWR_LD_PGOOD`
- LD monitor evaluates `LD_LPGD` and later LD analog telemetry
- TEC monitor evaluates `TEC_TEMPGD` and later TEC analog telemetry
- IMU monitor evaluates orientation safety
- trigger input monitor evaluates operator trigger state

This layer answers questions like:

- is the LD driver healthy?
- is the TEC controller healthy?
- is the orientation acceptable?
- is the operator trigger active?

### 5.3 System Status Layer

This layer stores the latest validated subsystem snapshots.

It provides a single shared view of:

- power status
- LD status
- TEC status
- IMU status
- fault latch status

The purpose of this layer is to decouple acquisition timing from supervisory decision timing.

### 5.4 Supervisor Layer

This is the core safety decision layer.

It performs:

- freshness checks
- power-health evaluation
- emission permit evaluation
- fault classification
- fault latching and clear policy
- DAC clamp policy
- state machine input generation

This layer is the final authority on whether the system may remain powered, whether the laser may emit, and whether the system must enter fault behavior.

### 5.5 State Machine and Output Layer

This layer expresses the global system state and the intended outputs associated with that state.

It controls:

- TEC power enable
- LD power enable
- LD operating mode through `SBDN`
- whether DAC outputs must be clamped

This layer converts supervisory decisions into stable system behavior.

## 6. MCU Interface Summary

This section describes the logic meaning of each important MCU interface.

### 6.1 System / Debug / Boot Interfaces

- `EN`: ESP32 enable/reset input; used for MCU reset, not part of runtime safety logic
- `GPIO0`: boot strap pin; reserved
- `GPIO46`: boot option pin; reserved
- `UART0 TX/RX`: used for logs, debug, and service visibility
- `USB D+ / D-`: USB connection path; not currently part of the laser safety loop

### 6.2 Power Control Interfaces

- `PWR_TEC_EN`: digital output; enables the TEC controller power domain
- `PWR_TEC_GOOD`: digital input; high means TEC power domain is good
- `PWR_LD_EN`: digital output; enables the LD driver power domain
- `PWR_LD_PGOOD`: digital input; high means LD driver power domain is good

These signals are part of the power-health chain, not merely the emission permit chain.

### 6.3 LD Driver Interfaces

- `LD_SBDN`: 3-level control signal for LD mode selection
- `LD_LPGD`: digital input; high means LD loop is healthy
- `LD_PCN`: digital output; selects the driver current branch
- `LD_TMO`: analog monitor output from LD driver
- `LD_LIO`: analog monitor output from LD driver

The design assumption for `LD_SBDN` is:

- low = shutdown
- high impedance = standby
- high = operate

The design assumption for `LD_PCN` is:

- the low-current branch is not used in this hardware revision
- the signal is fixed to the high branch selection

### 6.4 TEC Controller Interfaces

- `TEC_TEMPGD`: digital input; high means the TEC thermal condition is acceptable
- `TEC_TMO`: analog monitor output from TEC controller
- `TEC_ITEC`: analog monitor output representing TEC current
- `TEC_VTEC`: analog monitor output representing TEC voltage

### 6.5 IMU Interfaces

- SPI clock, MOSI, MISO, CS
- optional interrupt output from the IMU

The IMU is used to determine whether the laser is oriented within the allowed operating envelope.

The current design policy is:

- the laser is intended to operate downward
- if the beam direction rises beyond the allowed angle threshold, emission must be denied

### 6.6 Trigger Input

- one external GPIO is reserved as the trigger input
- trigger is treated as a digital operator request signal
- trigger itself never directly drives the laser
- trigger only becomes effective when the system is already in an allowed ready condition

### 6.7 DAC Interfaces

- DAC is connected by I2C
- one channel is intended for LD setpoint control
- one channel is intended for TEC setpoint control

The DAC is not the primary safety decision path. Instead, it is subordinate to the safety controller:

- safe state -> clamp DAC outputs to safe values
- firing state -> allow active setpoints

### 6.8 Reserved / Expansion Interfaces

Some GPIOs are routed to board-to-board connectors for later integration of:

- trigger
- distance sensor
- external sub-board logic
- potentially PD-related side functions

Those interfaces are intentionally kept outside the core logic until final board-level usage is confirmed.

## 7. Core States

The system state machine is defined logically as:

- `OFF`
- `POWERUP_TEC`
- `POWERUP_LD`
- `READY`
- `FIRING`
- `FAULT_INTERNAL`
- `FAULT_USAGE`

These states are used to describe the entire supervisory behavior.

### 7.1 OFF

Meaning:

- the system is held in safe inactive state
- both power enables are low
- laser driver is in shutdown
- DAC outputs are clamped to safe values

Purpose:

- boot-safe resting state
- recovery entry point after internal fault clear

### 7.2 POWERUP_TEC

Meaning:

- TEC power domain is being enabled
- LD power domain remains off
- laser emission is impossible

Exit condition:

- TEC power-good becomes valid and stable

Failure condition:

- TEC power-good does not arrive before timeout

### 7.3 POWERUP_LD

Meaning:

- TEC domain is already on
- LD power domain is now being enabled
- laser emission remains impossible

Exit condition:

- LD power-good becomes valid and stable

Failure condition:

- LD power-good does not arrive before timeout

### 7.4 READY

Meaning:

- both controller domains are powered
- all required safety conditions are continuously supervised
- laser is not emitting
- LD remains in standby, not operation

In this state the device is prepared for emission but has not yet been commanded to emit.

### 7.5 FIRING

Meaning:

- all required safety conditions are valid
- operator trigger is active
- LD is allowed to enter operation mode
- active DAC setpoints are allowed

This is the only state in which laser emission is logically allowed.

### 7.6 FAULT_INTERNAL

Meaning:

- an internal electrical, power, or control-path failure has occurred
- the system must be considered unsafe at the controller level

Output policy:

- TEC and LD power enables are dropped
- LD is shut down
- DAC outputs are clamped

Recovery policy:

- trigger must be released
- required clear condition must be satisfied
- system returns to `OFF`
- the full power-up sequence is executed again

### 7.7 FAULT_USAGE

Meaning:

- the operator or operating condition violated the approved usage envelope
- the system hardware itself is not necessarily damaged or unstable

Examples:

- unsafe IMU orientation
- future distance-out-of-range condition

Output policy:

- power domains remain enabled
- LD is shut down
- DAC outputs are clamped

Recovery policy:

- trigger must be released
- usage safety condition must become valid again
- system returns to `READY`

## 8. State Transition Logic

### 8.1 Startup Path

The normal startup path is:

`OFF -> POWERUP_TEC -> POWERUP_LD -> READY`

This means the system never jumps directly from reset into a firing-capable state.

### 8.2 Ready to Firing

The transition from `READY` to `FIRING` requires both:

- trigger asserted
- emission permit true

If either condition is absent, the system remains in `READY`.

### 8.3 Firing Exit

The transition from `FIRING` back to non-firing occurs immediately if:

- trigger is released
- permit is lost
- a fault condition is latched

### 8.4 Fault Priority

Internal fault has highest priority.

If both usage and internal faults exist at the same time:

- internal fault behavior dominates
- the state becomes `FAULT_INTERNAL`

This ensures electrical/power faults always override softer procedural faults.

## 9. Power-Up Safety Logic

Power-up is not treated as a single event. It is a supervised sequence.

### 9.1 Why the Power-Up Sequence Exists

The TEC controller and LD driver are separate hardware domains.

The firmware must confirm that:

- the TEC domain is present and stable
- the LD domain is present and stable
- the controllers are reporting healthy status

Only after those conditions are satisfied should the system present a ready state.

### 9.2 Sequence

The intended sequence is:

1. start in fully safe state
2. enable TEC power domain
3. wait for TEC `PGOOD`
4. enable LD power domain
5. wait for LD `PGOOD`
6. once both domains are good, evaluate runtime safety conditions
7. enter `READY`

### 9.3 Timeout Protection

Each power-up stage has its own timeout.

If expected power-good feedback does not arrive in time:

- the condition is treated as an internal fault
- the system transitions to `FAULT_INTERNAL`

This prevents indefinite half-powered operation.

## 10. Emission Permit Logic

Emission permit is the logical condition that determines whether laser operation is allowed.

The permit calculation requires that all critical monitored conditions are simultaneously true.

At the current design stage, permit requires:

- TEC power domain ready
- LD power domain ready
- TEC digital health good
- LD digital health good
- IMU orientation safe
- no internal fault latched
- no usage fault latched

The operator trigger is **not** part of permit itself.

Instead:

- permit answers "may the system emit if requested?"
- trigger answers "is the operator requesting emission now?"

Firing requires both.

## 11. Fault Classification

Faults are intentionally divided into two classes.

### 11.1 Internal Fault

Internal faults indicate controller-level or electrical instability.

Current internal fault sources include:

- power-good loss during runtime
- power-up timeout
- LD or TEC digital fault during firing

Future internal fault sources may include:

- analog overtemperature
- analog overcurrent
- DAC communication failure
- monitor task freshness failure

The design intention is that any internal fault implies:

- the powered control path cannot currently be trusted
- the safest recovery is to remove power and restart supervised power-up

### 11.2 Usage Fault

Usage faults indicate the device is being used outside the approved operating envelope, while hardware may still be functional.

Current usage fault source:

- unsafe IMU orientation during firing

Future usage fault sources may include:

- distance sensor out of range
- missing tissue contact condition
- operator procedural interlock failures

The design intention is that any usage fault implies:

- emission must stop immediately
- power may remain on
- the system may return to `READY` after the condition is corrected

## 12. Fault Detection and Reaction

### 12.1 Internal Fault Reaction

When an internal fault is detected:

- the fault is latched
- the state transitions to `FAULT_INTERNAL`
- both power enables are disabled
- LD is forced to shutdown
- DAC outputs are clamped to safe values

### 12.2 Usage Fault Reaction

When a usage fault is detected:

- the fault is latched
- the state transitions to `FAULT_USAGE`
- power remains enabled
- LD is forced to shutdown
- DAC outputs are clamped to safe values

### 12.3 Why Usage Fault Still Uses a Fault State

A usage fault is represented explicitly as a state instead of only denying permit.

This is important because:

- the UI can distinguish "ready but waiting" from "usage violation"
- logs can distinguish operator misuse from electrical faults
- recovery policy can be explicitly different from internal fault recovery

## 13. Fault Clear and Recovery Policy

### 13.1 Operator Release Requirement

Both fault classes require the operator trigger to be released and stable before recovery is allowed.

This prevents a held trigger from immediately re-entering firing the instant a fault clears.

### 13.2 Usage Fault Clear

A usage fault may clear when:

- the usage-safe condition is restored
- the trigger has been released long enough to pass debounce logic
- no internal fault is present

The system then returns to `READY`.

### 13.3 Internal Fault Clear

An internal fault may clear only when:

- the clear condition is satisfied
- the trigger has been released long enough to pass debounce logic

The system then returns to `OFF`, and must repeat:

`OFF -> POWERUP_TEC -> POWERUP_LD -> READY`

This intentionally forces revalidation of the power path.

### 13.4 Current Clear Condition Scope

At the present design stage, the clear condition is tied to restored operator-safe condition, especially IMU safety.

This should be refined later so each fault source can define its own clear policy.

Examples of future improvements:

- internal power faults may require explicit service clear
- thermal faults may require cooldown timing
- communication faults may require confirmed device response before clear

## 14. LD Output Control Logic

The laser driver is controlled through three logically distinct mechanisms.

### 14.1 Power Enables

- `PWR_LD_EN` controls whether the LD controller power domain is energized
- `PWR_TEC_EN` controls whether the TEC controller power domain is energized

These are part of system-level safety and startup supervision.

### 14.2 LD Mode Selection Through SBDN

The LD driver uses a 3-level control input.

Design meaning:

- `shutdown`: laser disabled
- `standby`: hardware powered and prepared, but not emitting
- `operate`: emission path allowed

Logical usage:

- `OFF`, `POWERUP_TEC`, `POWERUP_LD`, `FAULT_INTERNAL`, `FAULT_USAGE` -> `shutdown`
- `READY` -> `standby`
- `FIRING` -> `operate`

### 14.3 PCN Branch Selection

The current hardware design uses the high-current branch only.

Therefore:

- `PCN` is treated as a fixed configuration output
- it is not part of the active safety decision path

## 15. DAC Control Logic

The DAC controls analog setpoint references for LD and TEC related functions.

The DAC is subordinate to safety logic.

### 15.1 Safe Policy

When the system is not allowed to emit:

- DAC outputs are clamped to safe values

### 15.2 Ready Policy

In `READY`:

- the TEC channel may hold the standby/ready temperature setpoint
- the LD channel remains clamped or set to a non-emitting safe reference

### 15.3 Firing Policy

In `FIRING`:

- active LD setpoint is allowed
- TEC ready control may remain active

The exact raw DAC values are hardware calibration parameters, not design-logic parameters.

## 16. Real-Time Tasking Model

The firmware uses an RTOS-based periodic supervisory architecture.

The logic model is:

- fast digital monitors run periodically
- IMU acquisition runs periodically
- analog acquisition runs periodically
- supervisor runs at the highest control priority
- DAC update logic is event-driven from supervisory state changes

### 16.1 Why This Architecture Is Used

Different signals have different timing requirements.

- digital status lines are cheap to sample and should be checked often
- IMU data requires bus communication but still needs relatively fast updates
- analog telemetry can be slower
- final safety decisions should always run from a consistent snapshot

The architecture therefore separates sampling from decision-making.

### 16.2 Snapshot-Based Supervision

The supervisor should never mix partially updated information from multiple layers.

Instead:

- monitor tasks publish the latest subsystem snapshots
- supervisor reads the latest whole-system snapshot
- supervisor makes a coherent decision from that snapshot

This makes the timing model easier to reason about.

## 17. Freshness Rules

Every critical monitor source must be considered valid only if it has been updated recently enough.

If a monitor source becomes stale:

- it should no longer be trusted
- any safety decision that depends on it should fail safe

This protects against:

- task stalls
- bus failures
- silent sensor update loss

## 18. What Is Already Defined vs What Is Still Open

### 18.1 Defined at the Design Level

- overall role of the firmware
- state machine structure
- power-up sequencing concept
- trigger + permit firing rule
- internal vs usage fault classification
- fault reaction policy
- SBDN three-level control policy
- DAC subordinate safety policy
- major MCU interface mapping

### 18.2 Still Open at the Design Level

- final trigger GPIO and active polarity
- final distance-sensor interface and its fault role
- whether IMU alone is sufficient for usage-safe clear, or whether distance must also be included
- final analog threshold definitions
- analog voltage-to-physical-unit conversion formulas after board calibration
- whether some internal faults require explicit service intervention rather than auto-clear after release
- whether digital critical faults should also have direct interrupt-based emergency shutdown

## 19. Planned Next Design Additions

The following pieces are logically expected to be added after the current design stage:

- distance sensor logic integrated as a usage safety condition
- analog LD/TEC monitors integrated into internal fault classification
- explicit communication-failure handling for external devices
- optional interrupt-assisted fast shutdown for critical digital fault lines
- more detailed fault-source-specific clear policies

## 20. Design Summary

The firmware is designed as a real-time supervisory safety controller for a laser system with independent LD and TEC hardware controllers.

The design is based on the following principles:

- power supervision and emission permission are different decisions
- the laser may emit only when all required safety conditions are valid
- internal electrical faults and operator usage faults are not the same and must be handled differently
- startup must be sequenced and verified, not assumed
- recovery must require operator release and condition restoration
- every uncertainty must resolve to the safer behavior

The resulting logic is:

- boot safely
- power up in stages
- verify subsystem health
- hold the laser in standby while ready
- emit only when permit and trigger are both true
- classify faults by meaning
- react differently to internal versus usage violations
- recover only through controlled and validated transitions

This is the intended firmware design reference.
