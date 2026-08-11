# DESIGN_STEP2 — ADC / ePWM / Trip Zone Hardware Base

**Date:** 2026-08-10
**Project:** F28335_RTControl_Platform (PRD Step 2)
**Status:** Draft v1.0 — Implementation Stage

---

## 1. Pre-Verified Facts

### 1.1 PIE Vector Table Layout (Group 1)

| PIE MUX | Vector Symbol | Peripheral | Availability |
|---------|--------------|------------|-------------|
| INTx1 | SEQ1INT | ADC Sequencer 1 | **FREE — assigned to ADC ISR** |
| INTx2 | SEQ2INT | ADC Sequencer 2 | Free (reserved for future dual-sequencer) |
| INTx3 | rsvd1_3 | Reserved | — |
| INTx4 | XINT1 | External Int 1 | Free |
| INTx5 | XINT2 | External Int 2 | Free |
| INTx6 | ADCINT | ADC | Free |
| INTx7 | TINT0 | CPU Timer 0 | **OCCUPIED — Timer0 ISR (100 μs)** |
| INTx8 | WAKEINT | LPM/WD | Free |

**Conclusion:** ADC ISR binds to PIE1.1 (SEQ1INT). No conflict with Timer0 at PIE1.7.

### 1.2 TI GPIO Init Function Side-Effect Review

| Function | GPIOs Modified | MUX Registers Touched | Conflict with SCI/SPI? |
|----------|---------------|----------------------|------------------------|
| `InitEPwm1Gpio()` | GPIO0, GPIO1 | GPAMUX1 bits 0-1, 2-3 | **None.** SCI(35,36)=GPBMUX1; SPI(16,17,18)=GPAMUX2 |
| `InitTzGpio()` (default) | GPIO12 only | GPAMUX1 bits 24-25 | **None.** GPIO12 is in GPAMUX1; SPI pins are GPAMUX2; SCI pins are GPBMUX1 |
| `InitEPwm2-6Gpio()` | GPIO2-11 | GPAMUX1 bits 4-23 | **None** |
| `InitTzGpio()` (TZ5/TZ6 commented out) | GPIO16, GPIO17 | GPAMUX2 bits 0-1, 2-3 | **COMMENTED OUT.** These WOULD conflict with SPI MOSI/MISO if enabled |

**Verification:** TI's `InitEPwm1Gpio()` and `InitTzGpio()` are safe to call in the presence of existing SPI-A (GPIO16/17/18) and SCI-A (GPIO35/36). No register field overlap exists between any ePWM1/TZ1 GPIO and the SPI/SCI pins.

### 1.3 TBCLKSYNC Mechanism

- **Register:** `SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC` (bit 2)
- **TBCLKSYNC=0:** All ePWM time-base clocks stopped. TBCTR frozen. No PWM output generation possible regardless of GPIO mux or AQ settings.
- **TBCLKSYNC=1:** All ePWM time-base clocks enabled. Modules with their individual clock enables will start counting.
- **This is the primary hardware safety gate during initialization.**

### 1.4 Existing BSP Entry Point

The canonical init entry is `firmware/bsp/board.c:Board_Init()` (existing, NOT new). This function already orchestrates: MemCopy(ramfuncs) → DrvSysCtrl_Init → DrvFlash_Init → DrvInterrupt_Init → Timer0 → SCI → SPI → interrupts enable. Step 2 adds ADC/ePWM/TZ init into this same function — no new "big board.c" is created.

---

## 2. Safety Architecture

### 2.1 PWM Default-Off Chain (7 Layers)

```
Layer 1: TBCLKSYNC=0              — No time-base clock, TBCTR frozen
Layer 2: AQCSFRC (CSFA=CSFB=01)   — Continuous software force EPWMxA/B LOW
Layer 3: TZA=TZB=TZ_FORCE_LO       — Trip Zone action: force both outputs LOW
Layer 4: TZSEL (OSHT1=OSHT2=1)     — TZ1+TZ2 one-shot enabled (hardware trip)
Layer 5: CMPA=CMPB=0               — Compare values at safe minimum
Layer 6: AQCTLA/AQCTLB configured  — Normal AQ actions explicitly defined
Layer 7: GPIO mux set to ePWM      — Only AFTER layers 1-6 are confirmed

Enable sequence: TBCLKSYNC=1 → clear AQCSFRC → start counting
Disable sequence: AQCSFRC force LOW → stop counting → TBCLKSYNC optional
```

### 2.2 FREE_SOFT Policy

`TBCTL.FREE_SOFT` is configured to `00` (stop on emulation halt) for both Prototype and Industrial builds. This field describes **debugger-pause behavior only** and is never used for run-time power safety. All power safety is handled by AQCSFRC software force + TZ hardware trip.

### 2.3 Initialization Sequence (Strict Ordering)

```
Step 0: TBCLKSYNC=0 (already default after reset; confirmed explicitly)

Step 1: Configure ePWM registers while TBCTR is frozen:
  1a. TBPRD = period (safe value)
  1b. CMPA = 0, CMPB = 0 (safe minimum)
  1c. CMPCTL: LOADAMODE=CC_CTR_ZERO, SHDWAMODE=CC_SHADOW
  1d. AQCTLA: CAU=AQ_SET, CAD=AQ_CLEAR (normal PWM action)
  1e. AQCTLB: CBU=AQ_SET, CBD=AQ_CLEAR (normal PWM action)
  1f. AQCSFRC: CSFA=01 (force A LOW), CSFB=01 (force B LOW)
  1g. DBCTL: OUT_MODE=DB_FULL_ENABLE, POLSEL=DB_ACTV_HIC, IN_MODE=DBA_ALL
  1h. DBRED=DBFED=deadband_value
  1i. TZSEL: OSHT1=1, OSHT2=1 (or per BSP config)
  1j. TZCTL: TZA=TZ_FORCE_LO, TZB=TZ_FORCE_LO
  1k. TZEINT: OST=1 (enable one-shot interrupt for diagnostic)
  1l. Clear all TZ flags: TZCLR.OST=1, TZCLR.CBC=1, TZCLR.INT=1
  1m. ETSEL: SOCASEL=ET_CTR_ZERO, SOCAEN=1 (ADC trigger at CTR=ZERO)
  1n. ETPS: SOCAPRD=ET_1ST (trigger on every event)

Step 2: Configure ADC registers:
  2a. ADCTRL1: RESET=1, then clear (ADC reset sequence)
  2b. ADCTRL3: ADCCLKPS, ADCPWDN, ADCBGRFDN (power-up bandgap, set clock)
  2c. ADCTRL1: ACQ_PS, CPS, SEQ_CASC (acquisition, cascaded/dual mode)
  2d. ADCMAXCONV: MAX_CONV1=N-1 (for N channels)
  2e. ADCCHSELSEQ1-4: channel mapping per BSP config
  2f. ADCTRL2: EPWM_SOCA_SEQ1=1 (SOCA triggers SEQ1)

Step 3: Configure ADC ISR (PIE binding):
  3a. PieVectTable.SEQ1INT = &App_AdcIsr
  3b. PieCtrlRegs.PIEIER1.bit.INTx1 = 1 (SEQ1INT at PIE1.1)
  3c. ADCTRL2.INT_ENA_SEQ1=1, INT_MOD_SEQ1=0 (interrupt at every EOS)

Step 4: IF BOARD_PWM_ADC_HW_CONFIRMED:
  4a. Set GPIO mux for ePWM1A/1B (GPIO0,1 → MUX1)
  4b. Set GPIO mux for TZ1 (GPIO12 → MUX1)
  4c. Enable ePWM peripheral clock (if not already done)
  4d. DO NOT set TBCLKSYNC=1 yet — still gated by explicit enable

Step 5: Remain in disabled state. PWM outputs are held LOW by AQCSFRC.
```

### 2.4 Enable / Disable / Fault API Contract

```c
/* Enable PWM outputs — ONLY after safety preconditions verified.
 * Preconditions: TBCLKSYNC=0, AQCSFRC forcing LOW, TZ configured.
 * Post-conditions: TBCLKSYNC=1, AQCSFRC cleared, TBCTR starts counting. */
int32_t DrvEpwm_Enable(uint32_t epwm_module);

/* Disable PWM outputs — immediate safe state.
 * Action: set AQCSFRC force LOW first, then optionally stop TBCTR. */
void DrvEpwm_Disable(uint32_t epwm_module);

/* Query Trip Zone status. Returns bitmask of active trip sources. */
uint16_t DrvEpwm_GetTripStatus(uint32_t epwm_module);

/* Clear latched Trip Zone — ONLY after fault condition resolved.
 * Must re-validate preconditions before re-enabling. */
void DrvEpwm_ClearTrip(uint32_t epwm_module);
```

---

## 3. Board-Level Configuration

### 3.1 Hardware Confirmation Gate

```c
// board_config.h
#define BOARD_PWM_ADC_HW_CONFIRMED  0   // 0=unconfirmed, 1=confirmed

#if (PLATFORM_PROFILE_ACTIVE == PLATFORM_PROFILE_ID_INDUSTRIAL) && !BOARD_PWM_ADC_HW_CONFIRMED
#error "Industrial build requires BOARD_PWM_ADC_HW_CONFIRMED=1. Set only after hardware validation."
#endif
```

- When `BOARD_PWM_ADC_HW_CONFIRMED=0`: ADC/ePWM/TZ drivers can be initialized into safe-disabled state, host tests can run, but GPIO mux is NOT set for ePWM/TZ function. ADC hardware triggering is NOT enabled.
- When `BOARD_PWM_ADC_HW_CONFIRMED=1`: GPIO mux is set. Hardware proven safe. Real ADC sampling and PWM outputs can be enabled via explicit API.

### 3.2 Candidate Example Parameters (NOT validated on hardware)

All values below are marked `CANDIDATE` and are overridable via `board_config.h`.

| Parameter | Symbol | Default Value | Unit | Status |
|-----------|--------|---------------|------|--------|
| PWM module | BOARD_EPWM_MODULE | 1 | — | CANDIDATE |
| PWM EPWMxA pin | BOARD_PIN_EPWM1A | GPIO0 (Pin 5) | — | CANDIDATE |
| PWM EPWMxB pin | BOARD_PIN_EPWM1B | GPIO1 (Pin 6) | — | CANDIDATE |
| TZ1 input | BOARD_PIN_TZ1 | GPIO12 (Pin 21) | — | CANDIDATE |
| TZ2 input | BOARD_PIN_TZ2 | GPIO13 (Pin 24) | — | CANDIDATE |
| PWM frequency | BOARD_PWM_FREQ_HZ | 60000 | Hz | CANDIDATE |
| Dead-band RED | BOARD_PWM_DB_RED | 50 | TBCLK cycles | CANDIDATE (~333ns) |
| Dead-band FED | BOARD_PWM_DB_FED | 50 | TBCLK cycles | CANDIDATE (~333ns) |
| ADC channel 0 | BOARD_ADC_CH0 | 0 (ADCINA0) | — | CANDIDATE |
| ADC channel 1 | BOARD_ADC_CH1 | 8 (ADCINB0) | — | CANDIDATE |
| ADC clock (ADCCLK) | — | 12.5 | MHz | Derived: HSPCLK/6 |
| ADC sampling window | BOARD_ADC_ACQ_PS | 7 | ADCCLK | CANDIDATE (8 cycles) |
| SOC trigger point | BOARD_EPWM_SOCA_SEL | ET_CTR_ZERO | — | CANDIDATE |
| Max duty cycle | BOARD_PWM_MAX_DUTY | 0.48 | fraction | Hard clamp |

### 3.3 Responsibility Split

| Concern | Owner | File |
|---------|-------|------|
| Pin definitions | BSP | `firmware/bsp/board_pins.h` |
| Numeric clock/duty config | BSP | `firmware/bsp/board_config.h` |
| Register implementation | Driver | `firmware/drivers/drv_adc.c`, `drv_epwm.c` |
| Init orchestration | BSP | `firmware/bsp/board.c` |
| ISR glue | App | `firmware/app/isr.c` |
| PIE binding | Driver | `firmware/drivers/drv_interrupt.c` |

---

## 4. Driver API Design

### 4.1 drv_adc.h/.c

```c
typedef struct {
    uint32_t adc_clk_hz;         // ADCCLK frequency (e.g., 12.5MHz)
    uint16_t acq_ps;             // Acquisition window (ADCTRL1.ACQ_PS)
    uint16_t cps;                // Core clock prescaler (0=/1, 1=/2)
    uint16_t seq_mode;           // 0=dual, 1=cascaded (ADCTRL1.SEQ_CASC)
    uint16_t num_channels;       // Number of conversions (1-16)
    uint16_t channels[16];       // Channel selection array
    uint16_t interrupt_enable;   // Enable SEQ1 interrupt on EOS
} DrvAdcConfig;

/* Initialize ADC to powered-up, configured, but NOT triggered state.
 * Caller must separately bind ISR and enable triggers. */
int32_t DrvAdc_Init(const DrvAdcConfig *cfg);

/* Read raw ADC result (0-4095). Non-blocking, reads from result register.
 * Returns negative on invalid channel index. */
int32_t DrvAdc_ReadRaw(uint32_t channel);

/* Clear ADC interrupt flag. Call from ISR. */
void DrvAdc_ClearInterrupt(void);

/* Acknowledge SEQ1 interrupt in PIE. */
void DrvAdc_AckInterrupt(void);
```

### 4.2 drv_epwm.h/.c (includes Trip Zone)

```c
typedef struct {
    uint32_t tbclk_hz;           // Time-base clock (SYSCLKOUT=150MHz)
    uint32_t pwm_freq_hz;        // Target PWM frequency
    uint16_t count_mode;         // TB_COUNT_UP, TB_COUNT_UPDOWN, etc.
    uint16_t db_red;             // Dead-band rising edge delay (TBCLK cycles)
    uint16_t db_fed;             // Dead-band falling edge delay (TBCLK cycles)
    uint16_t tz_sources;         // TZSEL: which TZn inputs enabled (bitmask)
    uint16_t tz_oneshot_enable;  // TZSEL.OSHTn bits (one-shot trip enables)
    uint16_t tz_cbc_enable;      // TZSEL.CBCn bits (cycle-by-cycle trip enables)
} DrvEpwmConfig;

/* Initialize ePWM + Trip Zone into SAFE DISABLED state.
 * - All registers configured for safe operation
 * - AQCSFRC forces EPWMxA/B LOW
 * - TZ configured with TZ_FORCE_LO
 * - TBCTR frozen (TBCLKSYNC=0 if global sync note enforced externally)
 * - GPIO mux NOT set (caller controls via BSP based on HW_CONFIRMED)
 *
 * Returns 0 on success, negative on invalid config. */
int32_t DrvEpwm_Init(uint32_t module, const DrvEpwmConfig *cfg);

/* Set PWM compare A value (CMPA shadow register).
 * Value is clamped to [0, TBPRD * BOARD_PWM_MAX_DUTY].
 * Shadow update occurs at configured load point (CTR=ZERO). */
void DrvEpwm_SetCompareA(uint32_t module, uint16_t value);

/* Set PWM compare B value (CMPB shadow register). Clamped. */
void DrvEpwm_SetCompareB(uint32_t module, uint16_t value);

/* Enable PWM outputs after safety checks.
 * Clears AQCSFRC software force, starts TBCLKSYNC.
 * Requires BOARD_PWM_ADC_HW_CONFIRMED=1. */
int32_t DrvEpwm_Enable(uint32_t module);

/* Disable PWM outputs immediately.
 * Sets AQCSFRC force LOW, then stops TBCTR. */
void DrvEpwm_Disable(uint32_t module);

/* Query Trip Zone flags: returns TZFLG register value. */
uint16_t DrvEpwm_GetTripStatus(uint32_t module);

/* Clear cycle-by-cycle trip flag. */
void DrvEpwm_ClearCbcTrip(uint32_t module);

/* Clear one-shot trip flag. Requires explicit safety re-check before re-enabling. */
void DrvEpwm_ClearOstTrip(uint32_t module);

/* Get TBPRD (period register). */
uint16_t DrvEpwm_GetPeriod(uint32_t module);

/* Read TBCTR (counter). For diagnostic/debug only. */
uint16_t DrvEpwm_GetCounter(uint32_t module);
```

---

## 5. ADC ISR Design

### 5.1 ISR Flow

```
App_AdcIsr (PIE1.1 — SEQ1INT):
  1. Read cycle counter (WCET start)
  2. Read ADC result(s) via DrvAdc_ReadRaw() — N channels
  3. Safety check: raw values within sane bounds (e.g., not stuck at 0 or 4095 for multiple cycles)
  4. Dummy test computation (step 2 only): raw → clamped → write to CMPA/CMPB shadow
     - This is NOT closed-loop control. It is a safety-proven "pass-through" test.
     - All values clamped to [0, TBPRD * MAX_DUTY].
     - Only executed if PROTOTYPE profile and SAFE_OPENLOOP enabled.
  5. Update diagnostic counters: adc_isr_count, adc_raw_min/max per channel
  6. Clear ADC interrupt flag: ADCTRL2.INT_ENA_SEQ1 cleared → ADCST.INT_SEQ1_CLR=1
  7. PIE ACK: PieCtrlRegs.PIEACK.all = PIEACK_GROUP1
  8. WCET update
```

### 5.2 ISR Constraints (same as existing ISR policy)

- No printf, no formatting, no dynamic memory
- No service-layer calls
- No blocking loops or delay
- Only writes to diagnostics struct (ADC WCET slot, ADC telemetry)
- Does NOT touch UART/SPI state — no interference with communication

### 5.3 WCET Budget

| Phase | Operation | Estimated Cycles |
|-------|-----------|-----------------|
| Entry | Cycle read, overhead | ~5 |
| ADC read | N × DrvAdc_ReadRaw() | ~5 per channel |
| Safety check | Boundary compare | ~10 |
| Test compute | Clamp + write shadow | ~20 |
| Diagnostic | Update min/max/count | ~15 |
| ISR tail | Clear flags, PIE ACK, WCET | ~20 |
| **Total (1 ch)** | | **~75 cycles (~0.5 μs)** |
| **Total (4 ch)** | | **~90 cycles (~0.6 μs)** |

Budget target: <2 μs (well under 1% of 60kHz=16.7μs period).

---

## 6. Diagnostics Expansion

### 6.1 New WCET Slot

```c
typedef struct {
    // ... existing slots (timer0_isr, sci_rx_isr, main_loop) ...
    WcetSlot adc_isr;           // NEW: ADC ISR WCET
} Diagnostics;
```

### 6.2 New ADC/PWM/Trip Telemetry Fields

```c
typedef struct {
    // ... existing fields ...
    // --- Step 2 ADC/PWM/Trip telemetry (lightweight, ISR-safe) ---
    uint32_t adc_isr_count;         // Total ADC ISR invocations
    uint32_t adc_raw_samples[4];    // Last raw ADC values (up to 4 channels)
    uint32_t adc_stuck_zero_count;  // Count of samples == 0 (stuck-low detection)
    uint32_t adc_stuck_max_count;   // Count of samples == 4095 (stuck-high detection)
    uint16_t pwm_cmpa_shadow;       // Last written CMPA shadow value
    uint16_t pwm_cmpb_shadow;       // Last written CMPB shadow value
    uint16_t pwm_tz_flags;          // Last TZFLG snapshot
    uint16_t pwm_trip_count;        // Cumulative trip count
} Diagnostics;
```

---

## 7. Prototype vs Industrial Differences

| Capability | Prototype | Industrial |
|-----------|-----------|------------|
| PWM default OFF | Mandatory | Mandatory |
| Duty clamp [0, MAX_DUTY] | Mandatory | Mandatory |
| TZ force LO, one-shot | Mandatory | Mandatory |
| `BOARD_PWM_ADC_HW_CONFIRMED=0` | Allowed (safe-disabled) | **#error — build fails** |
| Safe open-loop test mode | Allowed via `PLATFORM_CAP_SAFE_OPENLOOP` | Compile-time disabled |
| Algorithm bypass | Allowed | Compile-time disabled |
| ADC ISR test compute | Allowed (clamped pass-through only) | Disabled (ISR reads ADC, updates diag, no PWM write) |
| ISR WCET diag | Full | Full |
| Debugger halt during PWM | Allowed via FREE_SOFT=00 (stop) | Allowed via FREE_SOFT=00 (stop) |

Both profiles share the identical 7-layer default-off chain and cannot bypass PWM default-off, duty clamp, or TZ.

---

## 8. Modified Files Inventory

| Action | File | Lines (est.) |
|--------|------|-------------|
| **NEW** | `docs/DESIGN_STEP2_ADC_PWM_TZ.md` | This file (~300 lines) |
| **NEW** | `firmware/drivers/drv_adc.h` | API header (~40 lines) |
| **NEW** | `firmware/drivers/drv_adc.c` | ADC register impl (~120 lines) |
| **NEW** | `firmware/drivers/drv_epwm.h` | ePWM+TZ API header (~60 lines) |
| **NEW** | `firmware/drivers/drv_epwm.c` | ePWM+TZ register impl (~250 lines) |
| MODIFY | `firmware/bsp/board_pins.h` | +6 lines (ADC, ePWM, TZ pin defines) |
| MODIFY | `firmware/bsp/board_config.h` | +30 lines (PWM freq, DB, ADC config, HW_CONFIRMED gate) |
| MODIFY | `firmware/bsp/board.c` | +40 lines (ADC/ePWM/TZ init in Board_Init) |
| MODIFY | `firmware/drivers/drv_interrupt.h` | +4 lines (ADC ISR bind/enable/ack declarations) |
| MODIFY | `firmware/drivers/drv_interrupt.c` | +15 lines (ADC ISR PIE1.1 support) |
| MODIFY | `firmware/app/isr.h` | +1 line (App_AdcIsr declaration) |
| MODIFY | `firmware/app/isr.c` | +50 lines (App_AdcIsr implementation) |
| MODIFY | `firmware/app/diagnostics.h` | +10 lines (ADC/PWM/Trip telemetry fields) |
| MODIFY | `firmware/app/diagnostics.c` | +8 lines (adc_isr slot init) |
| MODIFY | `firmware/app/main.c` | +15 lines (100ms ADC/PWM/Trip snapshot, WcetUpdate for adc_isr) |
| MODIFY | `linker/28335_RAM_lnk.cmd` | +2 lines (adc_buffer section placeholder) |
| MODIFY | `linker/f28335_flash.cmd` | +2 lines (adc_buffer section placeholder) |
| **NEW** | `tests/host/test_step2_clamp.c` | Host test (~80 lines) |
| MODIFY | `tests/host/_all.bat` | +2 lines (build + run step2 test) |
| MODIFY | `tools/quality_gate.ps1` | Update static boundary check for new drivers |
| **NEW** | `docs/STEP2_HARDWARE_TEST_PROCEDURE.md` | HW test procedure (~200 lines) |
| **NEW** | `docs/ACCEPTANCE_REPORT_STEP2.md` | Final acceptance report (after implementation) |

---

## 9. Sequence Diagram

### 9.1 Init Sequence (Board_Init expansion)

```
Board_Init()
├─ MemCopy(ramfuncs)                        [existing]
├─ DrvSysCtrl_Init()                         [existing]
├─ DrvFlash_Init()                           [existing, FLASH only]
├─ DrvInterrupt_Init()                       [existing]
├─ DrvTimer0_Init() + Bind + Enable          [existing]
├─ DrvSci_Init() + Bind + Enable             [existing]
├─ DrvSpi_Init()                             [existing]
│
├─ [NEW] TBCLKSYNC = 0 (confirm)
├─ [NEW] DrvEpwm_Init(1, &epwmCfg)          ← registers only, no GPIO mux
├─ [NEW] DrvAdc_Init(&adcCfg)               ← registers only, no trigger enable
├─ [NEW] DrvInterrupt_BindAdcSeq1(&App_AdcIsr)
├─ [NEW] DrvInterrupt_EnableAdcSeq1()
│
├─ [NEW] IF BOARD_PWM_ADC_HW_CONFIRMED:
│         InitEPwm1Gpio()                   ← set GPIO mux
│         InitTzGpio()                      ← set TZ GPIO mux
│
├─ [NEW] ALWAYS: remain in disabled state
│         (AQCSFRC forces LOW, TBCLKSYNC=0 until DrvEpwm_Enable)
│
└─ DrvInterrupt_EnableGlobal()              [existing]
   DrvTimer0_Start()                         [existing]
```

### 9.2 ADC ISR Timing (60 kHz PWM, up-down count)

```
PWM counter: 0 ──► 1250 ──► 0 ──► 1250 ...
                   (up)     (down)
SOCA trigger:  CTR=ZERO
ADC conversion: starts immediately after SOCA (~13 ADCCLK = ~1.04 μs)
ADC EOC → SEQ1INT: fires when conversion completes
App_AdcIsr: reads result, ~75 cycles (~0.5 μs)
CMPA/CMPB shadow update: written during ISR, loaded at next CTR=ZERO
```

---

## 10. Unresolved / PENDING Items

| # | Item | Dependency | Mitigation |
|---|------|-----------|------------|
| 1 | Actual PCB ADC channel mapping | Schematics / board inspection | Placeholder ADCINA0+B0; HW_CONFIRMED gate prevents accidental use |
| 2 | Power-stage PWM requirements (freq, db, topology) | Power board documentation | 60kHz candidate; all clamp limits configurable |
| 3 | Trip Zone source (overcurrent sensor, comparator) | Board schematic | TZ1+TZ2 defined but HW input path unverified |
| 4 | ADC external signal conditioning (gain, offset, filter) | Board schematic | Raw ADC output only; no calibration applied in Step 2 |
| 5 | ISR WCET measurement on real hardware | XDS100v3 + target board | Estimated ~0.5μs; measured value TBD |
| 6 | PWM waveform verification (freq, db, duty) | Oscilloscope + target board | PENDING until hardware available |
| 7 | TZ hardware trip verification | Oscilloscope + fault injection | PENDING until hardware available |
