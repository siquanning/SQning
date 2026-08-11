# DESIGN_STEP3 — Control, State, Parameters & Telemetry Platform

**Date:** 2026-08-10
**Project:** F28335_RTControl_Platform (PRD Step 3)
**Status:** Phase A — Design Complete, Implementation Ready

---

## 1. Pre-Verified Facts (Baseline Inventory)

### 1.1 Existing Data Ownership (Step 1 + Step 2)

| Data | Writer | Readers | Notes |
|------|--------|---------|-------|
| `g_sysTick` (uint32_t) | Timer0 ISR | Main loop (via `Timebase_Now()`) | 100µs increment |
| `SciRxQueue.items[]` | SCI RX ISR | Main loop | SPSC, lock-free |
| `SciRxQueue.write_index` | SCI RX ISR | Main loop | volatile |
| `SciRxQueue.read_index` | Main loop | SCI RX ISR | volatile |
| `SciRxQueue.overflow_count` | SCI RX ISR | Main loop (100ms) | volatile |
| `UartFrameContext.*` | Main loop (SpiBridge) | Diagnostics (100ms) | — |
| `SpiRequestContext.*` | Main loop (SpiBridge) | Diagnostics (100ms) | — |
| `Scheduler.{last_*, miss_*}` | Main loop | Diagnostics (100ms) | — |
| `Diagnostics.timer0_isr` | Timer0 ISR | Main loop (100ms) | WCET slot |
| `Diagnostics.sci_rx_isr` | SCI RX ISR | Main loop (100ms) | WCET slot |
| `Diagnostics.adc_isr` | ADC ISR | Main loop (100ms) | WCET slot |
| `Diagnostics.main_loop` | Main loop | Main loop (100ms) | WCET slot |
| `Diagnostics.adc_isr_count` | ADC ISR | Main loop (100ms) | — |
| `Diagnostics.adc_raw[0..1]` | ADC ISR | Main loop (100ms) | — |
| `Diagnostics.trip_flags` | ADC ISR | Main loop (100ms) | Step 2 |
| `Diagnostics.pwm_period` | Main loop (100ms) | — | Snapshot |
| `Diagnostics.pwm_counter` | Main loop (100ms) | — | Snapshot |
| `g_pSciRxQueue` | `App_IsrSetQueue()` | SCI RX ISR | Static pointer, boot-time immutable |
| `CMPA.half.CMPA / CMPB` (shadow) | DrvEpwm driver | ePWM HW (load at CTR=ZERO) | Through `DrvEpwm_SetCompareA/B` |
| `AQCSFRC` | `DrvEpwm_Init`, `Disable`, `Enable` | ePWM HW | Safety: held at Force LOW |

### 1.2 UART/SPI Command Assessment

The current UART→SPI bridge is a **transparent forwarding** protocol:
- UART frames are assembled into raw byte arrays
- Forwarded byte-by-byte over SPI
- **No command parsing, no parameter entry points exist**

The parameter entry API for Step 3 must be internally callable (by scheduler, state machine, or host test) but does NOT yet need a communication protocol binding. A `Param_SubmitPending()` API is the internal interface.

### 1.3 Scheduler Slot Allocation (Current)

| Slot | Period (ticks) | Period (time) | Current Use |
|------|---------------|----------------|-------------|
| 1ms | 10 | 1 ms | Empty — reserved |
| 10ms | 100 | 10 ms | Empty — reserved |
| 100ms | 1000 | 100 ms | Diagnostics snapshot |

### 1.4 ISR Vector Allocation (PIE Group 1)

| PIE MUX | Vector Symbol | Peripheral | Status |
|---------|--------------|------------|--------|
| INTx1 | SEQ1INT | ADC Sequencer 1 | **OCCUPIED — App_AdcIsr** |
| INTx2 | SEQ2INT | ADC Sequencer 2 | Free |
| INTx7 | TINT0 | CPU Timer 0 | **OCCUPIED — App_Timer0Isr** |

---

## 2. Component Relationship & Data Flow

### 2.1 Architecture After Step 3

```
┌─────────────────────────────────────────────────────────────┐
│  firmware/app/  (Composition Root)                           │
│  main.c  isr.c  scheduler.c  sci_rx_queue.c  app_context.c  │
│  diagnostics.c  state_machine.c  param_manager.c             │
│  ────────────────────────────────────────────────            │
│  Owns: main loop, ISR glue, multi-rate scheduler,           │
│        SPSC queue, WCET, system state, parameter lifecycle  │
└────┬──────────┬──────────┬──────────┬───────────────────────┘
     │          │          │          │
     ▼          ▼          ▼          ▼
┌─────────┐ ┌──────────┐ ┌──────────┐ ┌───────────────────────┐
│ services│ │ bsp/     │ │ control/ │ │ firmware/app/          │
│ (comm)  │ │ board.c  │ │ control_ │ │ telemetry.c            │
│         │ │          │ │ _faststep│ │                        │
│         │ │          │ │          │ │ Owns: telemetry sbuf   │
└────┬────┘ └────┬─────┘ └────┬─────┘ └───────────────────────┘
     │           │            │
     ▼           ▼            ▼
┌──────────────────────────────────────────────────────────────┐
│  firmware/drivers/                                           │
│  drv_epwm.c  drv_adc.c  drv_timer.c  drv_sci.c  drv_spi.c   │
│  drv_gpio.c  drv_sysctrl.c  drv_interrupt.c                  │
│  ────────────────────────────────────────────────             │
│  Owns: ALL peripheral register access                        │
└──────────────────────────────────────────────────────────────┘
```

### 2.2 Fast Control Path (ADC ISR)

```
ePWM SOCA (CTR=ZERO)
   ↓
ADC conversion
   ↓
ADC ISR (App_AdcIsr, PIE1.1):
  1. Read cycle counter (WCET start)
  2. DrvAdc_ReadRaw() — N channels
  3. Populate ControlInput from raw ADC values
  4. Control_FastStep(&g_control, &input, &output)
  5. If state == RUN && output.valid:
       DrvEpwm_SetCompareA/B(output.cmpa, output.cmpb) [shadow only]
  6. If output.fault_asserted:
       System_EnterFault(output.fault_code)
  7. Telemetry_WriteFastSnapshot(raw, output, state)
  8. Clear ADC flags, PIE ACK
  9. WCET update
```

### 2.3 Background Path (Main Loop via Scheduler)

```
1 ms task:
  - Param_CheckPendingCommit() — commit if cycle boundary ready
  - (reserved for future fast background services)

10 ms task:
  - StateMachine_Service() — evaluate transitions
  - Fault_Debounce() — confirm persistent faults
  - (reserved for slow outer loop)

100 ms task:
  - Diagnostics snapshot (existing)
  - Telemetry_ReadSnapshot() — consistent background copy
  - LED indicator update per system state
```

### 2.4 Data Flow Between ISR and Background

```
                ISR (writer)                Background (reader)
                ═══════════                 ═══════════════════
ControlContext  Control_FastStep()          (none — ISR-only)
  .state         reads active params         N/A
  .integrator    writes integrator           N/A

ControlOutput   set by Control_FastStep     Telemetry_ReadSnapshot()
  .cmpa, .cmpb   writes shadow registers     reads via snapshot
  .valid         sets valid/not              reads via snapshot

ParamManager    reads .active[]             Param_SubmitPending()
  .pending[]     NEVER writes               writes .pending[] via Param_CheckPendingCommit()
  .active[]      reads                       reads for diag

Telemetry       Telemetry_WriteFastSnapshot() Telemetry_ReadSnapshot()
  .fast[]        ISR-only writer             snapshot-taker (100ms)

SystemState     reads for RUN check         StateMachine_Service()
  .state         NEVER writes state           state transitions + fault entry
```

---

## 3. Control/Algorithm API Design

### 3.1 Control Types

```c
/* firmware/control/control_common.h */

typedef struct {
    uint16_t adc_raw[2];        /* Raw ADC samples (0-4095) */
    uint16_t vbus;              /* Derived bus voltage (scaled) */
    uint16_t iload;             /* Derived load current (scaled) */
    uint32_t reserved;          /* Future expansion */
} ControlInput;

typedef struct {
    uint16_t cmpa;              /* PWM compare A value (clamped) */
    uint16_t cmpb;              /* PWM compare B value (clamped) */
    uint16_t valid;             /* 1 = output valid for PWM update */
    uint16_t fault_code;        /* 0 = no fault, >0 = fault asserted */
    uint32_t fault_asserted;    /* Boolean: control detected fault */
} ControlOutput;

typedef struct {
    /* Active parameters (read-only for Control_FastStep) */
    uint16_t max_duty_permill;  /* Hard clamp 0-1000 */
    uint16_t deadband_ns;       /* Deadband in ns (diagnostic) */
    uint16_t control_mode;      /* 0=passthrough, 1=proportional_openloop (future) */
    uint16_t reserved;

    /* Internal state (Control_FastStep only) */
    uint16_t step_count;        /* Call counter for diagnostics */
    uint32_t acc_error;         /* Accumulated error (future integrator) */
} ControlContext;
```

### 3.2 Control_FastStep

```c
/* firmware/control/control_faststep.h */

/* Deterministic, non-blocking, fixed-cost pure function.
 * No TI headers, no registers, no global tick, no dynamic memory.
 * Called from ADC ISR at PWM frequency (e.g., 60 kHz).
 *
 * Preconditions:
 *   - context->max_duty_permill is within [0, 1000]
 *   - input->adc_raw[n] are valid ADC values (0-4095) or error sentinels.
 *
 * Postconditions:
 *   - output->cmpa, output->cmpb are clamped to [0, TBPRD * max_duty_permill/1000]
 *   - output->valid = 1 iff all inputs are within valid range
 *   - output->fault_asserted = 1 iff a control-level fault is detected
 *   - context->step_count is incremented (wraps at 16-bit)
 */
void Control_FastStep(ControlContext *context,
                      const ControlInput *input,
                      ControlOutput *output);
```

### 3.3 Minimal Algorithm: SafeOpenLoop (the only real algorithm needed now)

The SafeOpenLoop "algorithm" is a deterministic mapping:

```
For each ADC channel:
  1. Validate raw ADC ∈ [ADC_RAW_MIN_SAFE, ADC_RAW_MAX_SAFE]
     - If out of range: mark output invalid, assert low-level fault
  2. Apply linear scaling: raw → target duty fraction
  3. Clamp to [0, max_duty_permill]
  4. Convert permill → CMPA/CMPB value using TBPRD (passed via context)
```

This is the absolute minimum required to validate the ADC→Control→PWM shadow chain and provide a real testable algorithm. It is deterministic, bounded, and has clear pass/fail semantics.

```c
/* firmware/control/safe_openloop.h */

/* Safe open-loop mapping: ADC raw → clamped PWM compare value.
 * Pure function — no HW dependency, host-testable. */

/* Result of a single channel mapping */
typedef struct {
    uint16_t cmp_value;      /* Clamped compare value */
    uint16_t valid;          /* 1 = input was in valid range */
} SafeOpenLoopResult;

/* Map one ADC channel to a clamped PWM compare.
 *   raw_adc:       ADC reading (0-4095 valid, >4095 = error sentinel)
 *   tbprd:         PWM period register value
 *   max_duty_permill: hard upper limit (0-1000)
 *   gain_permill:     target mapping gain (0-1000 = 0%-100% of max_duty)
 * Returns clamped compare value + validity flag.
 */
SafeOpenLoopResult SafeOpenLoop_MapChannel(uint16_t raw_adc,
                                           uint16_t tbprd,
                                           uint16_t max_duty_permill,
                                           uint16_t gain_permill);
```

---

## 4. System State Machine

### 4.1 States

```c
typedef enum {
    SYSTEM_STATE_BOOT     = 0,  /* Power-on / reset — Board_Init() not yet complete */
    SYSTEM_STATE_INIT     = 1,  /* Board_Init() done, self-tests in progress */
    SYSTEM_STATE_STANDBY  = 2,  /* Self-tests passed, awaiting RUN command */
    SYSTEM_STATE_RUN      = 3,  /* Control active, ADC ISR running */
    SYSTEM_STATE_FAULT    = 4   /* Fault latched, PWM disabled, control inactive */
} SystemState;
```

### 4.2 Transition Table

| From | Trigger | To | Action |
|------|---------|----|--------|
| BOOT | `Board_Init()` complete | INIT | `StateMachine_Init()` called |
| INIT | Self-tests pass | STANDBY | Diagnostics ready, PWM in safe state |
| INIT | Self-test failure | FAULT | `System_EnterFault(FAULT_SELF_TEST)` |
| STANDBY | RUN command + preconditions | RUN | Control active; PWM still disabled if HW_CONFIRMED=0 |
| STANDBY | Fault detected | FAULT | `System_EnterFault(fault)` |
| RUN | Fault detected | FAULT | `System_EnterFault(fault)` → disable control output → disable PWM |
| RUN | STOP command | STANDBY | Control inactive, PWM disabled |
| FAULT | Clear + reset (Prototype only) | STANDBY | Clear latched fault, re-run self-tests |
| FAULT | (Industrial: permanent) | FAULT | Locked — requires power cycle |

### 4.3 State Transition Rules

1. **BOOT→INIT**: Automatic once `Board_Init()` returns and `StateMachine_Init()` is called from `main()`.
2. **INIT→STANDBY**: Automatic after self-test checks pass (PLL locked, Timer0 running, no driver init errors).
3. **STANDBY→RUN**: Explicit only. Requires all preconditions verified. In Prototype + HW_CONFIRMED=0, enters a "logical RUN" where control ISR executes but PWM outputs remain disabled (AQCSFRC force LOW, TBCLKSYNC=0).
4. **Any→FAULT**: Via `System_EnterFault()` — the ONLY fault entry path.
5. **FAULT→STANDBY**: Prototype only, via `PLATFORM_CAP_FAULT_QUICK_RESET`. Industrial builds lock FAULT permanently.

### 4.4 RUN in HW_CONFIRMED=0 (Logical RUN)

When `BOARD_PWM_ADC_HW_CONFIRMED == 0U`:
- The system CAN enter RUN state.
- ADC ISR executes: reads ADC, calls Control_FastStep, writes CMP shadow registers.
- `DrvEpwm_Enable()` is NEVER called.
- AQCSFRC continues forcing PWM outputs LOW.
- TBCLKSYNC remains 0 (no time-base clock).
- Diagnostic flag `DIAG_FLAG_LOGICAL_RUN_NO_HW` is set.
- This allows full software testing of the control path without hardware risk.

---

## 5. Fault Management

### 5.1 Fault Classification

```c
typedef enum {
    FAULT_NONE                        = 0,

    /* Hardware fast faults (Trip Zone or direct register read) */
    FAULT_HW_TZ_TRIP                  = 1,   /* ePWM TZFLG non-zero */
    FAULT_HW_ADC_STUCK_HIGH           = 2,   /* ADC stuck at 4095 for N consecutive samples */
    FAULT_HW_ADC_STUCK_LOW            = 3,   /* ADC stuck at 0 for N consecutive samples */
    FAULT_HW_PLL_LOCK_LOST            = 4,   /* PLL lock status lost */

    /* Software control faults (detected by Control_FastStep) */
    FAULT_SW_OUTPUT_SATURATION        = 10,  /* Output saturated at limit for too long */
    FAULT_SW_INPUT_OUT_OF_RANGE       = 11,  /* ADC input outside safe operating range */
    FAULT_SW_CONTROL_INVALID          = 12,  /* Control_FastStep produced invalid output */

    /* System / maintenance faults */
    FAULT_SYS_SELF_TEST_FAIL          = 20,  /* Init self-test failed */
    FAULT_SYS_PARAM_REJECTED          = 21,  /* Parameter validation failed (cumulative) */
    FAULT_SYS_SCHEDULER_MISS          = 22,  /* Scheduler miss count exceeded threshold */
    FAULT_SYS_ISR_WCET_EXCEEDED       = 23,  /* ISR WCET exceeded budget */

    /* Communication faults */
    FAULT_COMM_SPI_TIMEOUT_EXCESSIVE  = 30,  /* SPI timeouts exceeded threshold */
    FAULT_COMM_QUEUE_OVERFLOW         = 31   /* SCI RX queue overflow threshold exceeded */
} SystemFault;
```

### 5.2 Fault Entry (THE ONLY PATH)

```c
/* firmware/app/state_machine.h */

/* Enter FAULT state. Idempotent — first caller's fault is latched.
 * Actions:
 *   1. Record first_fault_code (only if not already in FAULT)
 *   2. Record fault_tick (Timebase_Now snapshot)
 *   3. Set state = SYSTEM_STATE_FAULT
 *   4. Set control output invalid flag
 *   5. Request PWM driver to enter safe state (AQCSFRC force LOW)
 *   6. Does NOT disable interrupts or halt the main loop
 *
 * This function is callable from ISR context.
 * It must NOT block, allocate, format, or call communication services.
 */
void System_EnterFault(SystemFault fault);
```

### 5.3 Fault Recovery

| Profile | Fault Type | Recovery |
|---------|-----------|----------|
| Prototype | Any | `System_ClearFault()` → re-enters STANDBY after self-tests |
| Industrial | HW fast faults | **LOCKED** — requires power cycle |
| Industrial | SW control faults | **LOCKED** — requires power cycle |
| Industrial | Comm faults | May clear after comm health restored (configurable) |

Industrial fault lock must be compile-time enforced and NOT bypassable via communication.

### 5.4 PWM Safe State on Fault

Every `System_EnterFault()` call MUST result in:
```
ControlOutput.valid = 0
DrvEpwm_Disable(BOARD_EPWM_MODULE)  → AQCSFRC force LOW
```

This is verified by: direct register readback test in host test (structural), and JTAG register inspection in HW test (pending).

---

## 6. Parameter Management

### 6.1 Parameter Model

```c
typedef struct {
    uint16_t version;        /* Monotonic version counter */
    uint16_t max_duty_permill;
    uint16_t control_mode;
    uint16_t tbprd;          /* Mirror of HW period for validation */
    uint16_t adc_safe_min;   /* Minimum valid ADC raw for each channel */
    uint16_t adc_safe_max;   /* Maximum valid ADC raw for each channel */
    uint16_t fault_thresh_adc_stuck;  /* Consecutive stuck samples before fault */
    uint16_t fault_thresh_sched_miss; /* Miss count before fault */
    uint16_t fault_thresh_spi_timeout;/* SPI timeout count before fault */
    uint16_t reserved[7];    /* Pad to power-of-2 size (16 words) */
} ControlParams;

typedef struct {
    ControlParams pending;   /* Submitted by background, NOT used by ISR */
    ControlParams active;    /* Read by ISR, written only at commit boundary */
    uint32_t     commit_requested;  /* Boolean: background requests commit */
    uint32_t     commit_count;      /* Total successful commits */
    uint32_t     reject_count;      /* Total rejected submissions */
    uint16_t     last_reject_reason;/* Reason code for last rejection */
} ParamManager;
```

### 6.2 Parameter Lifecycle

```
1. Background fills ParamManager.pending (via Param_SubmitPendingFields)
2. Background calls Param_RequestCommit() — sets commit_requested = 1
3. 1ms scheduler task calls Param_CheckPendingCommit():
   a. Validate range, version, consistency of pending
   b. If invalid: reject_count++, last_reject_reason = code, return
   c. If valid: copy pending → active (single memcpy — atomic at 16-bit word level)
   d. commit_count++, commit_requested = 0
4. ADC ISR reads ParamManager.active (NEVER pending)
```

### 6.3 Atomicity Guarantee

On C2000 (16-bit memory bus), a `memcpy` of 16 words (32 bytes) is not automatically atomic. The guarantee is:

- **Partial-read protection**: The ISR checks `active.version` before and after using active fields. If `version` changed between reads → ISR re-reads (spin-loop bounded to 2 iterations max since commit is infrequent).
- **Commit happens in scheduler (1ms)**: The ISR runs at 60 kHz (16.7µs). A 32-byte copy takes ~50 cycles (~0.33µs) and happens at a 1ms boundary — worst case overlap is 1 in 60, and the version double-check catches it.

### 6.4 Validation Rules

| Field | Rule |
|-------|------|
| `version` | Must be > active.version |
| `max_duty_permill` | Must be in [1, 1000] |
| `control_mode` | Must be 0 (passthrough) — only mode implemented |
| `tbprd` | Must be in [1, 65535] |
| `adc_safe_min` | Must be < adc_safe_max |
| `adc_safe_max` | Must be ≤ 4095 |
| Fault thresholds | Must be > 0 |

Industrial profile additionally requires:
- `max_duty_permill` ≤ 480 (48.0% hard cap) — cannot be raised at runtime
- `control_mode` MUST be 0 (passthrough only — no bypass modes)

---

## 7. Telemetry

### 7.1 Fast Snapshot (ISR Writes)

```c
typedef struct {
    uint16_t version;           /* Monotonic version, incremented each write */
    uint16_t state;             /* SystemState at sample time */
    uint16_t adc_raw[2];        /* Raw ADC samples */
    uint16_t cmpa;              /* CMPA value written */
    uint16_t cmpb;              /* CMPB value written */
    uint16_t output_valid;      /* ControlOutput.valid */
    uint16_t trip_flags;        /* TZFLG snapshot */
    uint16_t fault_code;        /* Active fault (0 if none) */
    uint16_t step_count;        /* Control_FastStep call count */
} TelemetryFastSnapshot;
```

ISR writes are fixed-cost:
- 10 × uint16_t writes = ~30 cycles
- No branching, no format conversion

### 7.2 Background Snapshot (Double-Buffer)

```c
typedef struct {
    TelemetryFastSnapshot buffer[2];  /* Double buffer */
    volatile uint16_t     active_idx; /* ISR writes to buffer[active_idx] */
    uint16_t              read_idx;   /* Background reads buffer[read_idx] */
    uint32_t              overrun_count; /* ISR write while background reading same idx */
} Telemetry;
```

### 7.3 Snapshot Protocol

```
ISR:
  idx = Telemetry.active_idx
  write fast fields to buffer[idx]
  buffer[idx].version++  (atomic 16-bit write as final step)
  (read_idx != idx may cause overrun if background is reading same buffer; acceptable)

Background (100ms):
  snapshot_version = buffer[active_idx].version  (read before)
  read_idx = active_idx  (swap: background now reads this buffer)
  read all fields from buffer[read_idx]
  verify buffer[read_idx].version == snapshot_version
  if mismatch → overrun_count++, retry next cycle
```

This guarantees the background never sees a half-written snapshot.

---

## 8. Prototype vs Industrial Behavior

| Feature | Prototype | Industrial |
|---------|-----------|------------|
| PWM default OFF | Mandatory | Mandatory |
| Duty clamp [0, MAX_DUTY] | Mandatory | Mandatory |
| TZ force LO | Mandatory | Mandatory |
| BOOT→INIT→STANDBY→RUN | Yes | Yes |
| Logical RUN (HW_CONFIRMED=0) | Allowed, `DIAG_FLAG_LOGICAL_RUN_NO_HW` set | Same (HW_CONFIRMED=0 blocks Run→PWM Enable identically) |
| FAULT recovery | `System_ClearFault()` → STANDBY | **FAULT locked permanently** (except comm faults) |
| Parameter `control_mode` | 0 (passthrough) only | 0 (passthrough) only — no bypass |
| Parameter `max_duty_permill` max | 1000 (can be any within limit) | 480 (48.0%) hard compile-time cap |
| Self-test on non-critical peripherals | Downgrade to warning | Must pass or fault |
| `SafeOpenLoop_MapChannel` | May use any gain_permill | gain_permill clipped to max_duty_permill |
| ISR WCET diag | Full | Full |
| Parameter reject counters | Yes | Yes |
| Debug bypass of fault lock | Allowed via `PLATFORM_CAP_FAULT_QUICK_RESET` | Compile-time disabled |

### 8.1 Unified Behavior (Both Profiles Identical)

- System_EnterFault() always disables PWM (AQCSFRC force LOW).
- DrvEpwm_Enable() always returns -2 when HW_CONFIRMED=0.
- TBCLKSYNC=0 until explicit DrvEpwm_Enable.
- Control_FastStep is identical — no profile-dependent behavior.
- Parameter validation rules enforced identically.
- Telemetry double-buffer mechanism identical.

---

## 9. Safety Boundaries Without Hardware (HW_CONFIRMED=0)

1. **PWM output pins**: AQCSFRC = Force LOW, TBCLKSYNC = 0 → no waveform possible.
2. **GPIO mux**: Not set for ePWM/TZ pins → GPIO remain as digital I/O.
3. **ADC triggers**: Not enabled → no hardware ADC conversions.
4. **`DrvEpwm_Enable()`**: Returns -2 → safe no-op.
5. **RUN state**: Logical only — ADC ISR is not firing on real hardware (no trigger).
6. **Fault path**: Fully testable in software — System_EnterFault disables control output and calls DrvEpwm_Disable.
7. **No simulated waveforms**: All test assertions work on software state, not register readback values.

---

## 10. File Inventory — New / Modified

### 10.1 New Files

| File | Lines (est.) | Purpose |
|------|-------------|---------|
| `firmware/control/control_common.h` | ~60 | ControlInput, ControlOutput, ControlContext typedefs |
| `firmware/control/control_faststep.h` | ~20 | Control_FastStep declaration |
| `firmware/control/control_faststep.c` | ~80 | Control_FastStep implementation (calls SafeOpenLoop) |
| `firmware/control/safe_openloop.h` | ~20 | SafeOpenLoop_MapChannel declaration |
| `firmware/control/safe_openloop.c` | ~50 | Minimal ADC→CMP safe mapping |
| `firmware/app/state_machine.h` | ~50 | SystemState enum, System_EnterFault, StateMachine_Service |
| `firmware/app/state_machine.c` | ~120 | State machine implementation + fault routines |
| `firmware/app/param_manager.h` | ~50 | ControlParams, ParamManager, submit/commit API |
| `firmware/app/param_manager.c` | ~100 | Validation + commit logic |
| `firmware/app/telemetry.h` | ~40 | TelemetryFastSnapshot, Telemetry, read/write API |
| `firmware/app/telemetry.c` | ~70 | ISR fast write + background snapshot |
| `tests/host/test_step3_control.c` | ~150 | Control + SafeOpenLoop tests |
| `tests/host/test_step3_state.c` | ~120 | State machine tests |
| `tests/host/test_step3_params.c` | ~100 | Parameter validation + commit tests |
| `tests/host/test_step3_telemetry.c` | ~80 | Telemetry consistency tests |
| `docs/DESIGN_STEP3_CONTROL_STATE_PARAMS_TELEMETRY.md` | This file | Design document |
| `docs/EVIDENCE_STEP3_SOFTWARE_VERIFICATION.md` | Phase F | Verification evidence |

### 10.2 Modified Files

| File | Changes |
|------|---------|
| `firmware/app/main.c` | +30 lines: init state_machine, param_manager, telemetry; scheduler task wiring |
| `firmware/app/isr.c` | +20 lines: ADC ISR calls Control_FastStep + Telemetry_WriteFastSnapshot |
| `firmware/app/diagnostics.h` | +15 lines: Step 3 diag fields (fault_code, logical_run, param_reject, telem_overrun) |
| `firmware/app/diagnostics.c` | +10 lines: init new fields |
| `firmware/app/app_context.h` | +10 lines: add control, state, param, telemetry to AppContext |
| `firmware/app/app_context.c` | +10 lines: init new submodules |
| `firmware/app/scheduler.h` | None (sufficient) |
| `firmware/app/scheduler.c` | None (sufficient) |
| `firmware/bsp/board_config.h` | +5 lines: Step 3 config constants |
| `firmware/platform_profile.h` | None (sufficient) |
| `tests/host/_all.bat` | +4 lines: build + run Step 3 tests |
| `tools/quality_gate.ps1` | +10 lines: add Step 3 test suites, update source list |
| `tools/build_all.ps1` | +2 lines: add control source files to build |

---

## 11. Test Plan

### 11.1 Host Tests (all new)

| Suite | Coverage |
|-------|---------|
| `test_step3_control` | SafeOpenLoop_MapChannel: normal/boundary/saturation/invalid ADC/zero-TBPRD/max-permill; Control_FastStep: valid input→valid output, invalid input→output.invalid, context state preservation, step_count wrap |
| `test_step3_state` | All transitions: BOOT→INIT→STANDBY→RUN→FAULT; FAULT cannot transition except ClearFault (Prototype); System_EnterFault latch (first fault preserved); Industrial FAULT lock; STANDBY→RUN precondition check; Fault disables control output |
| `test_step3_params` | Version monotonicity; range validation; commit atomicity via version double-check; reject counter; Industrial max_duty_permill cap; pending→active never partial |
| `test_step3_telemetry` | ISR write→background read consistency; version catch; overrun detection; double-buffer swap protocol |

### 11.2 Regression

- All Step 1+2 host tests must continue passing (UART frame, SPI request, SPI bridge, SCI queue, Step2 safety).
- UART/SPI protocol behavior unchanged — no new communication commands, no protocol modification.

### 11.3 Build Verification

- 4 configurations (Prototype_RAM, Prototype_Flash, Industrial_RAM, Industrial_Flash)
- All 0 error, 0 warning
- Static boundary check: no register access from control/, no DSP2833x includes from control/

---

## 12. Unresolved / PENDING Items

| # | Item | Dependency | Mitigation |
|---|------|-----------|------------|
| 1 | Control_FastStep WCET on real HW | XDS100v3 + target board | Host test validates functional; WCET pending HW |
| 2 | RUN state → actual PWM Enable | BOARD_PWM_ADC_HW_CONFIRMED=1 | Logical RUN fully testable; HW gate remains |
| 3 | FAULT recovery time measurement | HW + scope | Software path verified; timing pending |
| 4 | Parameter Flash persistence | Step 4 (not in scope) | API reserved for future; no fake implementation |
| 5 | Communication protocol for param submit | Future (CAN/I2C step) | Internal API (`Param_SubmitPendingFields`) is the entry |
| 6 | Additional control algorithms (PID, PR) | Future product need | Architecture supports; not needed now |
