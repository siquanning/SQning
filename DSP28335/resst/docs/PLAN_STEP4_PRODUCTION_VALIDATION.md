# PLAN_STEP4 — Production Validation & Reusable Release

**Date:** 2026-08-10
**Project:** F28335_RTControl_Platform (PRD Step 4)
**Status:** PLAN — awaiting hardware availability
**Author:** Claude Opus 4.7 (planning only; no hardware tests executed)

> This document defines the execution plan for PRD Step 4. All test statuses are PENDING / NOT RUN. No hardware tests have been executed. No firmware behavior has been modified. `BOARD_PWM_ADC_HW_CONFIRMED` remains `0U`.

---

## 1. Prerequisites & Safety Rules

### 1.1 Step 1–3 Completion Status

| Step | Software | Hardware | Overall |
|------|----------|----------|---------|
| Step 1 — Platform Identity | PASS (6/6 criteria) | PENDING (JTAG regression, Flash programming) | CONDITIONALLY COMPLETE |
| Step 2 — ADC/ePWM/TZ Base | PASS (4 configs build, 5 host suites, 4 static checks) | PENDING (7 items: ADC/PWM/Trip waveforms, WCET, HW_CONFIRMED) | CONDITIONALLY COMPLETE |
| Step 3 — Control/State/Params | PASS (8 host suites, 4 builds, quality gate) | PENDING (10 items: all require hardware) | CONDITIONALLY COMPLETE |

**Items from Steps 1–3 that remain PENDING and block Step 4:**

| # | Item | Blocks Step 4 Phase | Dependency |
|---|------|---------------------|------------|
| H1 | JTAG 1/64/65 byte regression (renamed project) | P7 (UART/SPI regression) | XDS100v3 + target board |
| H2 | Flash on-chip programming (Industrial_Flash_Release) | P1 (Flash boot) | XDS100v3 + target board |
| H3 | ADC waveform capture | P3, P4 | Oscilloscope + target board + HW_CONFIRMED=1 |
| H4 | ePWM output waveform (freq, DB, duty) | P2, P3, P5 | Oscilloscope + HW_CONFIRMED=1 |
| H5 | Trip Zone hardware trip | P5 | Oscilloscope + fault injection + HW_CONFIRMED=1 |
| H6 | ADC ISR WCET measurement | P4 | XDS100v3 + HW_CONFIRMED=1 |
| H7 | Timer0 ISR WCET measurement | P4 | XDS100v3 |
| H8 | SCI RX ISR WCET measurement | P4 | XDS100v3 |
| H9 | SPI timeout fault injection | P6 | Target board + CPLD disconnect |
| H10 | MISO anomaly fault injection | P6 | Target board + SPI bus fault |

### 1.2 Hardware Conditions Required Before Testing

Before ANY test that involves setting `BOARD_PWM_ADC_HW_CONFIRMED = 1U`, the following must be confirmed:

1. **PCB schematic review**: Confirm actual pin mapping for ADC channels (ADCINA0/B0 vs. actual PCB routing), ePWM pins (GPIO0/1 confirmed as EPWM1A/1B), and TZ pins (GPIO12/13 confirmed as TZ1/TZ2 with valid external pull and signal conditioning).
2. **Power stage isolation**: Confirm the PWM outputs are not connected to active power transistors, or that the power stage is fully de-energized with visible air gaps.
3. **CPLD connectivity**: Confirm SPI-A (GPIO16/17/18) is routed to the on-board CPLD and that the CPLD is powered and configured.
4. **JTAG connectivity**: XDS100v3 debug probe is recognized by CCS and can connect to the F28335 target.
5. **UART connectivity**: FTDI-based USB-to-UART adapter connected to SCI-A (GPIO35/36) at 3.3V TTL, recognized as a COM port.
6. **Boot pins**: GPIO84–87 are accessible and configurable (Flash boot = all high; SCI boot = 0010).

### 1.3 Mandatory Safety Measures

The following are non-negotiable for ALL tests beyond P0:

| # | Measure | Applies To |
|---|---------|------------|
| S1 | **Low-voltage / no-load power stage.** If a power stage is connected, it must be powered from a current-limited bench supply with the output disconnected or terminated into a safe dummy load. | P2–P8 |
| S2 | **Isolation power supply** for the DSP board separate from the power stage supply. | P2–P8 |
| S3 | **Current limit** set to the minimum the bench supply allows (e.g., 50 mA) for initial power-on. | P0–P2 |
| S4 | **Emergency stop** — a clearly identified power kill switch or bench supply output button within arm's reach. | All |
| S5 | **Oscilloscope probes grounded** at the DSP board ground, not at the power stage. Use differential probes for any floating measurements. | P2–P5 |
| S6 | **One change at a time.** Between tests, return PWM to safe state (AQCSFRC force LOW, TBCLKSYNC=0). | P2–P8 |
| S7 | **Two-person check** before first `BOARD_PWM_ADC_HW_CONFIRMED=1` build. Both must independently verify pin mapping against the schematic. | P2 |

### 1.4 Explicit Prohibitions

The following are FORBIDDEN until explicitly authorized by passing all prior phases:

- Setting `BOARD_PWM_ADC_HW_CONFIRMED = 1U` before P0 and P1 are complete.
- Calling `DrvEpwm_Enable()` before all P0–P2 safety checks pass.
- Connecting PWM outputs to a power stage before Trip Zone hardware trip is verified (P5).
- Running closed-loop control (PID, etc.) before the ADC sampling point is verified (P3).
- Leaving a running setup unattended without remote monitoring and an automatic shutdown trigger.
- Modifying `TBCLKSYNC`, `AQCSFRC`, or TZ configuration outside the defined driver API.
- Using Industrial_Flash_Release binary for any test without first confirming the build identity matches the DUT.

---

## 2. Phased Execution Sequence & Entry Conditions

### Phase Dependency Graph

```
P0: Tooling & Board Configuration
 │
 ├──► P1: Flash Programming & Standalone Boot
 │     │
 │     └──► P2: Default No-PWM Output & GPIO/PWM Baseline Waveforms
 │           │
 │           ├──► P3: ePWM SOCA & ADC Sampling Point
 │           │     │
 │           │     └──► P4: ADC ISR / Timer0 ISR / SCI RX ISR / Scheduler WCET & Jitter
 │           │           │
 │           │           └──► P5: Trip Zone & Software Fault Paths
 │           │                 │
 │           │                 ├──► P6: Communication Anomalies (SPI timeout, MISO, param reject)
 │           │                 │     │
 │           │                 │     └──► P7: UART→SPI 1/64/65 Byte Baseline Regression
 │           │                 │           │
 │           │                 │           └──► P8: 8–24 Hour Long-Run Stability
 │           │                 │
 │           │                 └──► P9: Release Review, Artifact Archival & Reproducibility
 │           │
 │           └──► (P5 independent of P4 ordering — can run in parallel if two setups available)
```

### P0: Tooling & Board-Level Configuration Confirmation

**Entry condition:** None (always safe — no firmware changes, no power applied to power stage).

**Objective:** Confirm all tools, instruments, cables, and board-level jumpers/DIP switches are correctly configured before any firmware is loaded.

**Steps:**

1. Visual inspection of the target board:
   - Confirm TMS320F28335 marking.
   - Locate and record all jumper positions.
   - Identify ADC input pins, ePWM output pins, TZ input pins, SPI pins, SCI pins, boot mode pins.
   - Photograph both sides of the board for reference.
2. Verify XDS100v3 is recognized by CCS (`getConnectedDevices`).
3. Verify USB-to-UART adapter COM port is visible (`listSerialPorts`).
4. Verify oscilloscope is functional with a probe compensation square wave.
5. Confirm boot pins (GPIO84–87) are set for Flash boot (all high) or are accessible for mode changes.
6. Document all findings in the test record template.

**Exit criteria:**
- Board photograph archived.
- Pin mapping documented (actual vs. schematic).
- All instruments confirmed operational.
- Test record P0 section completed.

**Failure recovery:** Correct tooling/configuration issues. No firmware involved — zero risk.

### P1: Flash Programming, JTAG Disconnect, Standalone Power-On Boot

**Entry condition:** P0 complete. XDS100v3 and target board confirmed.

**Objective:** Prove that Industrial_Flash_Release binary can be programmed to on-chip Flash, survives power cycle, and boots autonomously without JTAG.

**Hardware required:** XDS100v3, target board, USB-to-UART adapter, bench supply.

**Steps:**

1. Build `Industrial_Flash_Release` configuration:
   ```powershell
   cd E:\repos\DSP28335\F28335_RTControl_Platform\Release
   .\build.ps1 -Profile Industrial
   ```
2. Record build identity from map file:
   - `PLATFORM_BUILD_ID` string address
   - Git commit hash (from `git rev-parse HEAD`)
   - `.out` file SHA-256
   - `.map` file SHA-256
3. Verify in map file:
   - `codestart` at `0x33FFF6` (2 bytes)
   - `code_start` (DSP2833x_CodeStartBranch.obj) linked
   - `ramfuncs` LOAD in FLASHB, RUN in RAML03
   - CSM passwords all `0xFFFF` (unlocked)
4. Program Flash via CCS:
   - Set active build configuration to `Flash_Release`
   - Run → Debug (F11) — CCS programs Flash via XDS100v3
   - Verify programming completes without error
5. Verify post-program via CCS memory browser:
   - `0x33FFF6–0x33FFF7`: contains `LB _c_int00` opcode
   - `0x33FFF8–0x33FFFF`: all `0xFFFF`
6. JTAG disconnect test:
   - Run → Terminate (disconnect JTAG)
   - Power-cycle the board (remove and restore power)
   - Wait 2 seconds for boot
   - Open serial console at 9600 bps 8N1
   - Send 1-byte UART frame `A` (0x41)
   - Reconnect JTAG (hot-attach, do NOT reset)
   - Pause, read diagnostic counters
   - Verify `uart.rx_bytes ≥ 1` and `frames_forwarded ≥ 1`
7. Repeat steps 6 two more times (3 total power-cycle tests).

**Exit criteria:**
- Flash programming completes without error.
- Board boots from Flash and runs UART→SPI bridge after power cycle — 3/3 attempts.
- Diagnostic counters confirm UART frame was received and forwarded after standalone boot.
- P1 test record completed with map screenshots, build identity, and counter values.

**Failure recovery:**
- If codestart not at 0x33FFF6: check `--retain=code_start` in linker arguments.
- If board doesn't boot: verify boot pins (must be all high for Flash boot).
- If UART doesn't respond: hot-attach JTAG, check PLL locked, check SCI registers.
- **DO NOT** modify firmware to "fix" a Flash boot issue — diagnose root cause.

### P2: Default No-PWM Output & GPIO/PWM Baseline Waveforms

**Entry condition:** P1 complete. Oscilloscope available. **BOARD_PWM_ADC_HW_CONFIRMED still 0U.**

**Objective:** Confirm that with HW_CONFIRMED=0, PWM output pins remain LOW under all conditions. Then, under controlled conditions, verify PWM waveform parameters after a temporary, closely-monitored HW_CONFIRMED=1 build.

**Hardware required:** XDS100v3, target board, oscilloscope (≥100 MHz, ≥2 channels), probes.

**Safety note:** This is the first phase where PWM GPIO pins are configured for ePWM function. All tests start with the safety lock engaged and only release it under direct observation.

**Sub-phase P2a — Default-Off Confirmation (HW_CONFIRMED=0):**

1. Build `Prototype_RAM_Debug` (HW_CONFIRMED=0, safety lock active).
2. Load via JTAG, resume.
3. Probe GPIO0 (EPWM1A) and GPIO1 (EPWM1B) with oscilloscope.
4. Confirm both pins are LOW (<0.5V) continuously for ≥10 seconds.
5. Enter RUN state via state machine (already auto-transitions in main.c).
6. Confirm pins remain LOW (AQCSFRC force LOW, TBCLKSYNC=0).
7. Read `diag_flags` via JTAG — confirm `DIAG_FLAG_LOGICAL_RUN_NO_HW` is set.
8. **Result: If any PWM output is observed, STOP. Do not proceed.** Investigate register state.

**Sub-phase P2b — PWM Waveform Verification (temporary HW_CONFIRMED=1):**

1. **Safety pre-check:** Confirm power stage is disconnected/de-energized. PWM pins go ONLY to oscilloscope probes.
2. Temporarily set `BOARD_PWM_ADC_HW_CONFIRMED = 1U` in `board_config.h`.
3. Rebuild `Prototype_RAM_Debug`.
4. Load via JTAG. **Do NOT resume yet.**
5. Set breakpoint at `DrvEpwm_Enable()` call site (or step through manually).
6. Resume. The system will boot to logical RUN.
7. **Before** allowing `DrvEpwm_Enable()`:
   - Verify AQCSFRC still holds force LOW (read via CCS register view).
   - Verify oscilloscope shows both pins LOW.
8. Call `DrvEpwm_Enable(1)` via CCS Expressions or step through.
9. **Immediately** observe oscilloscope:
   - EPWM1A: square wave, frequency = 60 kHz ±1%
   - EPWM1B: complementary square wave (if dead-band enabled) with dead-band gap
   - Measure: period, duty cycle (initially ~5% from safe passthrough), dead-band RED/FED
10. Read TBCLKSYNC, AQCSFRC, TBCTR, CMPA, CMPB via CCS register view.
11. Call `DrvEpwm_Disable(1)` via CCS Expressions.
12. Confirm oscilloscope shows both pins return to LOW immediately.
13. **Restore `BOARD_PWM_ADC_HW_CONFIRMED = 0U`** in `board_config.h`.
14. Rebuild and confirm default-off behavior again (P2a repeat).

**Measurements to record (oscilloscope screenshots):**
- P2a: GPIO0 and GPIO1 at 1V/div, 1ms/div — confirm constant LOW
- P2b: EPWM1A at 2V/div, 5μs/div — full period with frequency annotation
- P2b: EPWM1A + EPWM1B at 2V/div, 500ns/div — dead-band gap measurement (rising + falling edges)
- P2b: EPWM1A at 2V/div, 5μs/div after `DrvEpwm_Disable()` — confirm immediate return to LOW

**Exit criteria:**
- P2a: Both PWM pins confirmed LOW in all states (BOOT, INIT, STANDBY, RUN) with HW_CONFIRMED=0.
- P2b: PWM frequency 60 kHz ±1%, dead-band ~333ns per side, immediate disable confirmed.
- HW_CONFIRMED restored to 0U after test.
- P2 test record completed with oscilloscope screenshots.

**Failure recovery:**
- If PWM frequency wrong: check TBPRD calculation and TBCLK divider.
- If dead-band wrong: check DBRED/DBFED register values and verify TBCLK=SYSCLKOUT.
- If disable doesn't work: do NOT proceed — AQCSFRC/DISABLE path must work before any further tests.
- **If any unexpected PWM output with HW_CONFIRMED=0: STOP EVERYTHING.** This indicates a register safety failure.

### P3: ePWM SOCA & ADC Sampling Point

**Entry condition:** P2 complete. ADC input signal source available (function generator or known DC voltage).

**Objective:** Verify that ePWM SOCA triggers ADC at the correct point in the PWM cycle, and that ADC samples are taken at the configured acquisition window.

**Hardware required:** XDS100v3, oscilloscope (≥100 MHz, ≥3 channels), function generator or DC source, probes.

**Sub-phase P3a — SOCA Trigger Timing:**

1. Temporarily set `BOARD_PWM_ADC_HW_CONFIRMED = 1U`.
2. Build `Prototype_RAM_Debug`, load via JTAG.
3. Configure oscilloscope:
   - CH1: EPWM1A (GPIO0) — PWM output
   - CH2: ADC SOC signal (use a GPIO toggle in ADC ISR as proxy if direct SOC observation unavailable)
   - Trigger: CH1 rising edge
4. Enable PWM via `DrvEpwm_Enable(1)`.
5. Observe: ADC ISR GPIO toggle should occur at a fixed delay after PWM period start (CTR=ZERO).
6. Measure: delay from PWM rising edge to ADC ISR entry (GPIO toggle).
7. Confirm: ADC ISR fires once per PWM period (60 kHz).

**Sub-phase P3b — ADC Sampling Verification:**

1. Apply known DC voltage to ADCINA0 (e.g., 1.65V = mid-scale ≈ 2048 counts).
2. Read `diag->adc_raw[0]` via JTAG over 100 samples.
3. Verify: values cluster around expected ADC code (1.65V / 3.3V × 4095 ≈ 2048) ± noise.
4. Repeat for ADCINB0 with a different DC voltage.
5. Apply a low-frequency sine wave (e.g., 100 Hz, 0–3V) to ADCINA0.
6. Read `diag->adc_raw[0]` via JTAG at 100ms intervals for 20 samples — values should track the sine envelope.

**Exit criteria:**
- ADC ISR fires at PWM frequency (60 kHz), synchronized to CTR=ZERO.
- ADC readings are within expected range for known input voltages.
- HW_CONFIRMED restored to 0U after test.

**Failure recovery:**
- If ADC ISR doesn't fire: check ETSEL.SOCAEN, ETSEL.SOCASEL, PIE1.1 binding.
- If ADC readings are noise: check ADC clock divider, acquisition window, signal conditioning.
- If ADC readings stuck at 0 or 4095: check channel selection, ADCTRL3 power bits.

### P4: ADC ISR, Timer0 ISR, SCI RX ISR, Scheduler WCET & Jitter

**Entry condition:** P3 complete.

**Objective:** Measure worst-case execution time (WCET) and jitter for all ISRs and main loop segments using the existing CPU Timer2 cycle counter infrastructure.

**Hardware required:** XDS100v3, USB-to-UART adapter, oscilloscope (optional, for GPIO-toggling jitter method).

**WCET Infrastructure (already in place):**
- CPU Timer2: 150 MHz free-running down-counter (`Diagnostics_CycleRead()`)
- `WcetSlot` per context: `timer0_isr`, `sci_rx_isr`, `adc_isr`, `main_loop`
- 1 cycle = 6.67 ns at 150 MHz

**Sub-phase P4a — Idle WCET Baseline:**

1. Build `Prototype_RAM_Debug` (HW_CONFIRMED=0 — safe).
2. Load via JTAG, resume, let run for 60 seconds with no UART traffic.
3. Pause, read all four WcetSlots via JTAG at 0xC2C0:
   ```
   timer0_isr: min/max/last
   sci_rx_isr: min/max/last
   adc_isr:    min/max/last (will be 0 — ADC ISR doesn't fire with HW_CONFIRMED=0)
   main_loop:  min/max/last
   ```
4. Record values in WCET template.

**Sub-phase P4b — Loaded WCET (UART Stress):**

1. Resume from P4a state (counters continue accumulating).
2. Send continuous 64-byte UART frames at maximum sustainable rate (≥10 frames/second).
3. Run for 60 seconds under load.
4. Pause, read all WcetSlots — record max values as loaded WCET.

**Sub-phase P4c — WCET with ADC ISR Active (HW_CONFIRMED=1):**

1. **Only if P3 passed** — temporarily set `BOARD_PWM_ADC_HW_CONFIRMED = 1U`.
2. Rebuild, load, enable PWM.
3. Run for 60 seconds under combined ADC + UART load.
4. Pause, read all WcetSlots.
5. Record `adc_isr.max_cycles` as ADC ISR WCET.
6. Restore `BOARD_PWM_ADC_HW_CONFIRMED = 0U`.

**Sub-phase P4d — GPIO-Toggle Jitter Measurement (optional but recommended):**

1. Add a temporary GPIO toggle at ADC ISR entry/exit (different GPIO pin).
2. Measure time between consecutive ISR entries on oscilloscope.
3. Record min/max period → jitter = max_period − min_period.

**Analysis:**

Convert cycles to microseconds at 150 MHz:

| ISR | Max Cycles | Time (μs) | Period / Budget | Budget % |
|-----|-----------|-----------|-----------------|----------|
| Timer0 ISR | TBD | TBD | 100 μs / N/A (tick only) | N/A |
| SCI RX ISR | TBD | TBD | N/A (event-driven) | N/A |
| ADC ISR | TBD | TBD | 16.7 μs (60 kHz) | TBD % |
| Main Loop | TBD | TBD | N/A (background) | N/A |

**Exit criteria:**
- All four WCET slots have valid min/max/last values.
- ADC ISR WCET < 2.0 μs (target: <12% of 16.7 μs period; design estimate: ~0.6 μs).
- Timer0 ISR WCET < 1.0 μs (target: <1% of 100 μs period).
- SCI RX ISR WCET < 50 μs (target: <5% of 1.04 ms byte time).
- WCET records completed with sample count and test conditions.

### P5: Trip Zone & Software Fault Paths

**Entry condition:** P2 complete. P4 recommended but not strictly required (Trip Zone is independent of WCET).

**Objective:** Verify that TZ1/TZ2 hardware trip shuts down PWM independently of software, and that software fault paths correctly disable PWM.

**Hardware required:** XDS100v3, oscilloscope (≥2 channels), pull-up resistor (e.g., 10kΩ to 3.3V), jumper wire for TZ pin toggling.

**Safety note:** Trip Zone tests are inherently safe — the objective IS to shut down PWM. However, ensure the power stage is disconnected before starting.

**Sub-phase P5a — TZ1 Hardware Trip (One-Shot):**

1. Temporarily set `BOARD_PWM_ADC_HW_CONFIRMED = 1U`.
2. Build `Prototype_RAM_Debug`, load via JTAG.
3. Configure oscilloscope:
   - CH1: EPWM1A
   - CH2: TZ1 (GPIO12)
   - Trigger: CH2 falling edge (TZ active-low expected)
4. Enable PWM via `DrvEpwm_Enable(1)`.
5. Confirm PWM is running on CH1.
6. Pull TZ1 LOW (connect GPIO12 to GND via jumper).
7. Observe: CH1 must go LOW within <100 ns (hardware trip, not software).
8. Read TZFLG register via CCS — confirm OST1 flag set.
9. Read `diag->trip_flags` — confirm non-zero.
10. Read `diag->fault_code` — confirm `FAULT_HW_TZ_TRIP` latched.
11. Attempt `DrvEpwm_Enable(1)` — must fail (TZ still latched).
12. Call `DrvEpwm_ClearOstTrip(1)`.
13. Attempt `DrvEpwm_Enable(1)` — must succeed (TZ cleared).
14. Restore `BOARD_PWM_ADC_HW_CONFIRMED = 0U`.

**Sub-phase P5b — Software Fault Path (ADC Stuck-High Simulation):**

1. With HW_CONFIRMED=0 (safe), inject a simulated ADC stuck-high condition:
   - Set a breakpoint in ADC ISR, manually overwrite `ctrl_output.fault_asserted = 1`.
2. Resume. Observe:
   - `System_EnterFault()` is called.
   - `state_machine.state` transitions to `SYSTEM_STATE_FAULT`.
   - `pwm_disable_requested` is set.
   - At next 1ms task, `DrvEpwm_Disable()` is called.
3. Read fault diagnostics via JTAG:
   - `diag->fault_code` = expected fault code
   - `diag->fault_tick` = non-zero
   - `diag->system_state` = FAULT (4)
4. In Prototype: call `System_ClearFault()` via CCS expression, verify transition to STANDBY.
5. Verify `ClearFault` from non-FAULT state is rejected.

**Sub-phase P5c — PWM Safe State After Any Fault:**

For each fault path, verify PWM outputs return to LOW:
- FAULT_HW_TZ_TRIP
- FAULT_SW_CONTROL_INVALID
- FAULT_SYS_SCHEDULER_MISS (inject by forcing miss counter high)
- FAULT_COMM_SPI_TIMEOUT_EXCESSIVE (inject by forcing timeout counter high)

**Exit criteria:**
- TZ1/TZ2 hardware trip turns off PWM within one PWM cycle (hardware path, not ISR).
- OST flag is latched and prevents re-enable until explicitly cleared.
- All software fault paths result in `AQCSFRC` force LOW.
- Fault code is correctly latched (first fault preserved).

### P6: Communication Anomalies — SPI Timeout, MISO Anomaly, Parameter Rejection

**Entry condition:** P1 complete (UART→SPI link functional). P5 recommended.

**Objective:** Verify that communication fault paths detect, report, and safely handle anomalies without crashing or corrupting state.

**Hardware required:** XDS100v3, USB-to-UART adapter. CPLD target board.

**Sub-phase P6a — SPI Timeout Injection:**

1. Load `Prototype_RAM_Debug` via JTAG, resume.
2. Physically disconnect MISO line (GPIO17) — this causes SPI RX buffer to never receive data.
3. Send a 1-byte UART frame.
4. Observe: SPI byte transfer starts (SPITXBUF written) but never completes.
5. After 5 ms: `spi.timeouts` must increment.
6. After cumulative timeouts exceed `fault_thresh_spi_timeout` (default in active params):
   - `FAULT_COMM_SPI_TIMEOUT_EXCESSIVE` is asserted at the 100ms task.
7. Read diagnostics to confirm fault latched.
8. Reconnect MISO. Clear fault (Prototype). Verify normal operation resumes.

**Sub-phase P6b — MISO Anomaly Detection:**

1. With CPLD connected and responding normally:
2. Send a 1-byte UART frame.
3. Read `spi.miso_unexpected` → should increment (CPLD responds with non-0xFF).
4. (If CPLD can be configured to respond with 0xFF): verify `spi.miso_idle_ff` increments.

**Sub-phase P6c — Parameter Rejection:**

1. Via CCS Expressions, attempt to submit invalid parameters:
   - `max_duty_permill = 0` → must reject (REJECT_RANGE).
   - `max_duty_permill = 1001` → must reject.
   - `tbprd = 0` → must reject.
   - `adc_safe_min >= adc_safe_max` → must reject.
   - `version <= active.version` → must reject (REJECT_VERSION).
2. After each rejection:
   - `param_manager.reject_count` increments.
   - `param_manager.last_reject_reason` matches expected reject code.
   - `param_manager.active` is NOT modified.
3. Submit a valid parameter set with `version = active.version + 1`.
4. Verify `param_manager.commit_count` increments and `active` is updated.
5. At next ADC ISR firing, verify `ControlContext_SyncFromActive()` picks up the new values.

**Exit criteria:**
- SPI timeout correctly detected and counted.
- Excessive SPI timeouts trigger fault entry.
- MISO anomaly counters function correctly.
- All invalid parameter combinations are rejected with correct reason codes.
- Valid parameters commit atomically.

### P7: UART→SPI 1/64/65 Byte Baseline Regression

**Entry condition:** P1 complete.

**Objective:** Re-establish the UART→SPI bridge regression baseline using the current `F28335_RTControl_Platform.out` binary (not the old DROOP name). This closes the Step 1 hardware PENDING item.

**Hardware required:** XDS100v3, USB-to-UART adapter, CPLD target board.

**Test procedure:** Identical to the existing baseline tests in [BASELINE_TEST_RECORD.md](../tests/hardware/BASELINE_TEST_RECORD.md), but using:
- Current `F28335_RTControl_Platform.out` binary
- Current diagnostic counter paths (context-based, not global variables)

**Sub-phase P7a — JTAG Regression (1/64/65 byte):**

1. Load `Prototype_RAM_Debug` via JTAG, resume.
2. Zero all counters (fresh load).
3. **Test 1-byte:** Send `A` (0x41) × 1.
   - Expect: `uart.rx_bytes +1`, `uart.ready_frames +1`, `spi.req_frames +1`, `spi.req_bytes +1`, `frames_forwarded +1`.
4. **Test 64-byte:** Send `B` (0x42) × 64.
   - Expect: `uart.rx_bytes +64`, `uart.ready_frames +1`, `spi.req_bytes +64`.
5. **Test 65-byte:** Send `C` (0x43) × 65.
   - Expect: `uart.rx_bytes +65`, `uart.ready_frames 0`, `uart.too_long_frames +1`, `spi.req_bytes 0`.
6. Record all counter deltas and compare against historical baseline.

**Sub-phase P7b — Flash Standalone Regression:**

1. Program `Industrial_Flash_Release` via CCS.
2. Power-cycle, boot standalone (no JTAG).
3. Connect serial console.
4. Execute 1/64/65 byte tests via serial console.
5. Hot-attach JTAG, read counters.
6. Verify results match JTAG-loaded baseline.

**Exit criteria:**
- 1/64/65 byte regression matches historical baseline within expected tolerance (counter access paths changed but values should be identical).
- Flash standalone regression matches JTAG regression.
- Step 1 hardware PENDING item can be closed.

### P8: 8–24 Hour Long-Run Stability

**Entry condition:** P1–P7 all complete. Flash standalone boot confirmed.

**Objective:** Prove the platform runs stably for extended periods without deadlock, memory corruption, counter overflow, or unexplained error accumulation.

**Hardware required:** Target board (Flash-programmed), USB-to-UART adapter, bench supply, PC with serial logging.

**Setup:**

1. Program `Industrial_Flash_Release` to target board.
2. Connect USB-to-UART adapter. Connect oscilloscope to a spare GPIO if available (for heartbeat monitoring).
3. Configure serial logging script on PC:
   - Send a 64-byte test frame every 500 ms (2 frames/second).
   - Log timestamp and any error indicators.
4. Power-cycle the board (standalone boot).
5. Record start time, initial diagnostic counters (via initial JTAG hot-attach snapshot, then disconnect).

**Monitoring (every 2 hours or at test end):**

1. Hot-attach JTAG (do NOT reset), pause, read diagnostic struct at 0xC2C0:
   - `sci_rx_total` — bytes received; must match expected rate × elapsed time
   - `sci_rx_overflow` — must be 0
   - `miss_1ms, miss_10ms, miss_100ms` — must not grow (occasional single misses acceptable under external interrupt load; sustained growth = failure)
   - `adc_isr_count` — (0 if HW_CONFIRMED=0; matches PWM freq × elapsed if HW_CONFIRMED=1)
   - `param_reject_count` — must be 0 (no parameter submissions during test)
   - `telem_overrun_count` — must be 0
   - `fault_code` — must be FAULT_NONE (0)
   - `system_state` — must be RUN (3)
   - `diag_flags` — must show expected state
2. Read SPI bridge counters:
   - `uart.rx_bytes`, `uart.ready_frames`, `uart.too_long_frames`, `uart.busy_drops`, `uart.uart_errors`
   - `spi.req_frames`, `spi.req_bytes`, `spi.timeouts`, `spi.start_failures`
   - `frames_forwarded`
3. Verify conservation law: `rx_bytes = spi.req_bytes + busy_drops` (within test tolerance).

**Stop conditions (any of these = test FAIL):**
- `fault_code != FAULT_NONE`
- `sci_rx_overflow > 0` (sustained)
- `spi.timeouts` growing without bound
- `miss_1ms` or `miss_10ms` growing without bound
- Board stops responding to UART (deadlock / hang)
- Conservation law violation > 1%

**Data to save at each checkpoint:**
- Full JTAG memory dump of `g_diagnostics` (0xC2C0, 44 bytes)
- Full JTAG memory dump of SPI bridge counters (addresses from map file)
- Timestamp and elapsed time

**Exit criteria:**
- 8 hours minimum continuous run with zero faults, zero overflows, zero unexplained error growth.
- 24 hours preferred for Industrial release candidate.
- All conservation laws hold throughout.
- P8 test record completed with checkpoint data.

### P9: Release Review, Artifact Archival & Reproducibility

**Entry condition:** P1–P8 all PASS.

**Objective:** Produce the final release package — binaries, map files, build logs, test records, and documentation — such that a new engineer can reproduce the build and verify the results.

**No hardware required.** This is a documentation and archival phase.

**Steps:**

1. **Final clean build** of all 4 configurations from a clean git checkout:
   ```
   git clean -fdx
   Build all 4 configs via quality_gate.ps1 or build_all.ps1
   ```
2. **Record build identity** for each configuration:
   - Git commit SHA (full 40-char)
   - Compiler version (`cl2000 --version`)
   - Build command line
   - `.out` SHA-256
   - `.map` SHA-256
   - `PLATFORM_BUILD_ID` string
3. **Archive release artifacts** in a dated directory:
   ```
   releases/v1.0.0-<date>/
   ├── Prototype_RAM_Debug/
   │   ├── F28335_RTControl_Platform.out
   │   ├── F28335_RTControl_Platform.map
   │   └── build.log
   ├── Prototype_Flash_Demo/
   ├── Industrial_RAM_Debug/
   ├── Industrial_Flash_Release/
   ├── test_records/
   │   ├── STEP4_P0_tooling.md
   │   ├── STEP4_P1_flash_boot.md
   │   ├── ... (all P0–P9 records)
   │   └── STEP4_WCET_RECORD.csv
   ├── waveforms/
   │   └── (oscilloscope screenshots, organized by phase)
   ├── CHECKLIST_Prototype_Demo.md
   ├── CHECKLIST_Industrial_Release.md
   └── KNOWN_LIMITATIONS.md
   ```
4. **Verify reproducibility:**
   - On a different PC (or after `git clean -fdx`), repeat the build.
   - Compare `.out` SHA-256 — must match (deterministic build).
   - If `.out` differs, compare `.map` — explain differences (timestamps, paths).
5. **Git tag** (plan only — do not execute):
   ```
   git tag -a v1.0.0 -m "F28335_RTControl_Platform v1.0.0 — Step 4 production validation"
   ```
   Tag naming: `v<MAJOR>.<MINOR>.<PATCH>` per `PLATFORM_VERSION_*` in `platform_profile.h`.

---

## 3. Complete Test Matrix

### Test ID Convention

`S4-<PHASE>-<NN>` where PHASE = P0–P9, NN = sequential within phase.

### P0: Tooling & Board Configuration

| Field | Value |
|-------|-------|
| **ID** | S4-P0-01 |
| **PRD Reference** | Step 4 §"完成Flash实际烧写" |
| **Dependencies** | None |
| **Objective** | Confirm board visual inspection, photograph, jumper documentation |
| **Pass Criteria** | Board photographed, jumpers documented, pin locations identified |
| **Prerequisites** | Physical access to target board |
| **Hardware** | Camera, board schematic (if available) |
| **Procedure** | Visual inspection per P0 steps |
| **Evidence** | Board photos, jumper table, pin map |
| **Failure Criteria** | Unable to identify key pins or jumpers |
| **Safety Action** | None (no power) |
| **Status** | PENDING |
| **Result** | — |
| **Operator** | — |
| **Date** | — |
| **Firmware SHA** | — |

| Field | Value |
|-------|-------|
| **ID** | S4-P0-02 |
| **PRD Reference** | NFR-03, NFR-05 |
| **Dependencies** | None |
| **Objective** | Verify XDS100v3, USB-to-UART, oscilloscope operational |
| **Pass Criteria** | XDS100v3 connects in CCS; COM port visible; scope shows cal square wave |
| **Prerequisites** | PC with CCS 20.5.1+, oscilloscope |
| **Hardware** | XDS100v3, USB-to-UART, oscilloscope, probes |
| **Procedure** | `getConnectedDevices`, `listSerialPorts`, scope probe compensation |
| **Evidence** | Screenshots of connected devices, COM port list, scope cal waveform |
| **Failure Criteria** | Any instrument not detected or malfunctioning |
| **Safety Action** | None |
| **Status** | PENDING |
| **Result** | — |
| **Operator** | — |
| **Date** | — |
| **Firmware SHA** | — |

| Field | Value |
|-------|-------|
| **ID** | S4-P0-03 |
| **PRD Reference** | Step 2 §3.2 (candidate pin mapping) |
| **Dependencies** | S4-P0-01 |
| **Objective** | Confirm ADC, PWM, TZ, SPI, SCI, boot pin mapping against schematic/PCB |
| **Pass Criteria** | Each signal in board_config.h confirmed against actual PCB trace or schematic |
| **Prerequisites** | Board photos, schematic |
| **Hardware** | Multimeter (continuity), schematic |
| **Procedure** | Continuity test from F28335 pin to header/test point for each signal |
| **Evidence** | Pin mapping table with "confirmed" / "needs investigation" per signal |
| **Failure Criteria** | Any critical signal (ADC, PWM, TZ) cannot be confirmed |
| **Safety Action** | None (board unpowered for continuity tests) |
| **Status** | PENDING |
| **Result** | — |

### P1: Flash Standalone Boot

| Field | Value |
|-------|-------|
| **ID** | S4-P1-01 |
| **PRD Reference** | Step 4 §"Flash断电独立启动通过" |
| **Dependencies** | S4-P0-03, Step 1 H2 |
| **Objective** | Build Industrial_Flash_Release, verify map, program Flash |
| **Pass Criteria** | Build 0E 0W; codestart at 0x33FFF6; ramfuncs LOAD/RUN correct; programming successful |
| **Prerequisites** | P0 complete |
| **Hardware** | XDS100v3, target board |
| **Software** | CCS 20.5.1+, build.ps1 |
| **Procedure** | Per P1 steps 1–5 |
| **Evidence** | Build log, map screenshot (codestart, ramfuncs, CSM), .out/.map SHA-256 |
| **Failure Criteria** | Build error, codestart missing, programming error |
| **Safety Action** | None (no power stage connected) |
| **Status** | PENDING |
| **Result** | — |

| Field | Value |
|-------|-------|
| **ID** | S4-P1-02 |
| **PRD Reference** | Step 4 §"Flash断电独立启动通过" |
| **Dependencies** | S4-P1-01 |
| **Objective** | Power-cycle standalone boot × 3, verify UART bridge functional |
| **Pass Criteria** | 3/3 boots successful; UART frame received and forwarded after each boot |
| **Prerequisites** | Flash programmed (S4-P1-01) |
| **Hardware** | Target board, USB-to-UART, bench supply |
| **Procedure** | Per P1 steps 6–7 |
| **Evidence** | Serial console log, JTAG counter readout after each boot |
| **Failure Criteria** | Any boot fails to respond to UART |
| **Safety Action** | Power off, check boot pins, re-check Flash programming |
| **Status** | PENDING |
| **Result** | — |

| Field | Value |
|-------|-------|
| **ID** | S4-P1-03 |
| **PRD Reference** | Step 4 §"Flash Release register consistency" |
| **Dependencies** | S4-P1-02 |
| **Objective** | Verify peripheral registers after Flash standalone boot match Debug build baseline |
| **Pass Criteria** | PLLSTS, HISPCP, LOSPCP, SCICCR, SCIHBAUD, SCILBAUD, SPICCR, SPIBRR, SPIFFTX, CpuTimer0 TCR all match |
| **Prerequisites** | Flash standalone boot confirmed |
| **Hardware** | XDS100v3 (hot-attach) |
| **Procedure** | Hot-attach JTAG after standalone boot, read registers, compare against HARDWARE_TEST.md §3.8 baseline |
| **Evidence** | Register value table with match column |
| **Failure Criteria** | Any register mismatch |
| **Safety Action** | Investigate root cause; do not proceed to P2 |
| **Status** | PENDING |
| **Result** | — |

### P2: PWM Baseline Waveforms

| Field | Value |
|-------|-------|
| **ID** | S4-P2-01 |
| **PRD Reference** | Step 2 §"PWM上电默认关闭", NFR-02 |
| **Dependencies** | S4-P0-03 |
| **Objective** | Confirm PWM pins LOW in all states with HW_CONFIRMED=0 |
| **Pass Criteria** | GPIO0/1 < 0.5V continuously in BOOT/INIT/STANDBY/RUN states |
| **Prerequisites** | Prototype_RAM_Debug loaded via JTAG |
| **Hardware** | XDS100v3, oscilloscope (≥2 channels) |
| **Procedure** | Per P2a |
| **Evidence** | Oscilloscope screenshots × 4 states, DIAG_FLAG_LOGICAL_RUN_NO_HW confirmed |
| **Failure Criteria** | Any PWM activity observed with HW_CONFIRMED=0 |
| **Safety Action** | **IMMEDIATE STOP** — investigate AQCSFRC, TBCLKSYNC, GPIO mux |
| **Status** | PENDING |
| **Result** | — |

| Field | Value |
|-------|-------|
| **ID** | S4-P2-02 |
| **PRD Reference** | Step 2 §"PWM频率、死区、触发点和ADC采样点符合配置" |
| **Dependencies** | S4-P2-01 |
| **Objective** | Verify PWM frequency, dead-band, and duty cycle with temporary HW_CONFIRMED=1 |
| **Pass Criteria** | f=60 kHz ±1%, DB RED/FED ≈ 333 ns each, CMPA/CMPB values correct |
| **Prerequisites** | P2a passed |
| **Hardware** | XDS100v3, oscilloscope (≥2 channels, ≥100 MHz) |
| **Procedure** | Per P2b |
| **Evidence** | Oscilloscope screenshots: full period, dead-band zoom, disable response |
| **Failure Criteria** | Frequency out of spec, dead-band missing or wrong polarity, disable fails |
| **Safety Action** | Call `DrvEpwm_Disable()`, power off, check register configuration |
| **Status** | PENDING |
| **Result** | — |

### P3: ADC Sampling Point

| Field | Value |
|-------|-------|
| **ID** | S4-P3-01 |
| **PRD Reference** | Step 2 §"ADC ISR与PWM周期同步，无软件轮询触发" |
| **Dependencies** | S4-P2-02 |
| **Objective** | Verify ADC ISR fires at PWM frequency synchronized to CTR=ZERO |
| **Pass Criteria** | ADC ISR period = 16.67 μs ±1%, aligned to PWM period start |
| **Prerequisites** | HW_CONFIRMED=1, PWM enabled |
| **Hardware** | XDS100v3, oscilloscope |
| **Procedure** | Per P3a |
| **Evidence** | Oscilloscope screenshot: PWM + ADC ISR GPIO toggle |
| **Failure Criteria** | ADC ISR not firing, wrong frequency, jitter >1% |
| **Safety Action** | Disable PWM, check ETSEL/ADC trigger configuration |
| **Status** | PENDING |
| **Result** | — |

| Field | Value |
|-------|-------|
| **ID** | S4-P3-02 |
| **PRD Reference** | FR-02, Step 2 §"ADC原始值、PWM比较值、Trip和ISR WCET诊断" |
| **Dependencies** | S4-P3-01 |
| **Objective** | Verify ADC readings for known DC and AC inputs |
| **Pass Criteria** | DC: readings within ±5% of expected; AC: readings track sine envelope |
| **Prerequisites** | ADC ISR confirmed firing |
| **Hardware** | Function generator or DC source, oscilloscope |
| **Procedure** | Per P3b |
| **Evidence** | Table of 100 samples for DC, 20 samples for AC sine |
| **Failure Criteria** | Readings stuck, noise >10%, wrong channel |
| **Safety Action** | Disable PWM, check ADC channel mapping and signal conditioning |
| **Status** | PENDING |
| **Result** | — |

### P4: WCET & Jitter

| Field | Value |
|-------|-------|
| **ID** | S4-P4-01 |
| **PRD Reference** | NFR-01, Step 4 §"所有关键ISR有真实WCET和裕量记录" |
| **Dependencies** | S4-P1-02 (UART link functional) |
| **Objective** | Measure Timer0 ISR and SCI RX ISR WCET (idle) |
| **Pass Criteria** | Timer0 ISR < 1.0 μs, SCI RX ISR < 50 μs |
| **Prerequisites** | Prototype_RAM_Debug loaded, WCET infrastructure active |
| **Hardware** | XDS100v3 |
| **Procedure** | Per P4a |
| **Evidence** | WCET record table with min/max/last in cycles and μs, sample count |
| **Failure Criteria** | WCET exceeds budget; max_cycles = 0 (counter not running) |
| **Safety Action** | None (HW_CONFIRMED=0) |
| **Status** | PENDING |
| **Result** | — |

| Field | Value |
|-------|-------|
| **ID** | S4-P4-02 |
| **PRD Reference** | NFR-01, Step 4 |
| **Dependencies** | S4-P4-01 |
| **Objective** | Measure WCET under UART load |
| **Pass Criteria** | SCI RX ISR max increase ≤2× idle; main loop max remains < budget |
| **Prerequisites** | Idle WCET baseline recorded |
| **Hardware** | XDS100v3, USB-to-UART |
| **Procedure** | Per P4b |
| **Evidence** | Loaded WCET record table |
| **Failure Criteria** | SCI RX ISR exceeds 100 μs; main loop exceeds 100 μs |
| **Safety Action** | None |
| **Status** | PENDING |
| **Result** | — |

| Field | Value |
|-------|-------|
| **ID** | S4-P4-03 |
| **PRD Reference** | Step 2 §"快速ISR WCET已测量并满足目标预算" |
| **Dependencies** | S4-P3-01 |
| **Objective** | Measure ADC ISR WCET |
| **Pass Criteria** | ADC ISR WCET < 2.0 μs (target: <12% of 16.7 μs period) |
| **Prerequisites** | ADC ISR confirmed firing (S4-P3-01) |
| **Hardware** | XDS100v3, oscilloscope |
| **Procedure** | Per P4c |
| **Evidence** | ADC ISR WCET record, GPIO-toggle jitter measurement |
| **Failure Criteria** | ADC ISR exceeds 3.3 μs (20% of period) |
| **Safety Action** | Disable PWM, optimize ISR or reduce control complexity |
| **Status** | PENDING |
| **Result** | — |

### P5: Trip Zone & Fault Paths

| Field | Value |
|-------|-------|
| **ID** | S4-P5-01 |
| **PRD Reference** | FR-04, Step 2 §"Trip Zone可独立于软件关闭PWM并锁存原因" |
| **Dependencies** | S4-P2-02 |
| **Objective** | Verify TZ1 one-shot hardware trip |
| **Pass Criteria** | PWM shuts down within <100 ns of TZ1 LOW; OST flag latched; DrvEpwm_Enable blocked until clear |
| **Prerequisites** | HW_CONFIRMED=1, PWM enabled |
| **Hardware** | XDS100v3, oscilloscope, pull-up resistor, jumper |
| **Procedure** | Per P5a |
| **Evidence** | Oscilloscope screenshot (PWM + TZ1), TZFLG register readout |
| **Failure Criteria** | PWM does not shut down; shutdown delayed >1 PWM cycle; TZ clear doesn't work |
| **Safety Action** | **CRITICAL** — Trip Zone is the last-resort hardware protection. If it fails, the platform cannot be used with a power stage. |
| **Status** | PENDING |
| **Result** | — |

| Field | Value |
|-------|-------|
| **ID** | S4-P5-02 |
| **PRD Reference** | FR-07, Step 3 §"所有RUN→FAULT路径都能使PWM进入安全状态" |
| **Dependencies** | S4-P5-01 |
| **Objective** | Verify software fault paths → PWM disable |
| **Pass Criteria** | Each fault path: state → FAULT, pwm_disable_requested=1, DrvEpwm_Disable called at 1ms task |
| **Prerequisites** | State machine functional (verified in host tests) |
| **Hardware** | XDS100v3 |
| **Procedure** | Per P5b–P5c |
| **Evidence** | Fault diagnostic readout per fault type, AQCSFRC register readback |
| **Failure Criteria** | Any fault path does not result in PWM disable |
| **Safety Action** | Fix fault handler before proceeding to any power-stage testing |
| **Status** | PENDING |
| **Result** | — |

| Field | Value |
|-------|-------|
| **ID** | S4-P5-03 |
| **PRD Reference** | FR-11 (Industrial fault lock) |
| **Dependencies** | S4-P5-02 |
| **Objective** | Verify Industrial build locks FAULT permanently (except comm faults) |
| **Pass Criteria** | `System_ClearFault()` returns error or is compile-time disabled in Industrial; comm fault may clear |
| **Prerequisites** | Industrial_RAM_Debug or Industrial_Flash_Release build |
| **Hardware** | XDS100v3 |
| **Procedure** | Induce fault in Industrial build, attempt ClearFault via CCS expression |
| **Evidence** | State remains FAULT after ClearFault attempt |
| **Failure Criteria** | ClearFault succeeds for non-comm fault in Industrial build |
| **Safety Action** | Fix Industrial fault lock enforcement |
| **Status** | PENDING |
| **Result** | — |

### P6: Communication Anomalies

| Field | Value |
|-------|-------|
| **ID** | S4-P6-01 |
| **PRD Reference** | Step 4 §"执行Trip Zone、SPI超时、MISO异常和参数拒绝故障注入" |
| **Dependencies** | S4-P1-02 |
| **Objective** | SPI timeout detection and fault escalation |
| **Pass Criteria** | timeout counter increments after MISO disconnect; FAULT_COMM_SPI_TIMEOUT_EXCESSIVE asserted after threshold |
| **Prerequisites** | UART→SPI link functional |
| **Hardware** | XDS100v3, USB-to-UART, CPLD board (MISO disconnectable) |
| **Procedure** | Per P6a |
| **Evidence** | Counter readout before/after disconnect, fault diagnostic |
| **Failure Criteria** | Timeout not detected; fault not asserted |
| **Safety Action** | Reconnect MISO, clear fault |
| **Status** | PENDING |
| **Result** | — |

| Field | Value |
|-------|-------|
| **ID** | S4-P6-02 |
| **PRD Reference** | Step 4, FR-08 |
| **Dependencies** | S4-P1-02 |
| **Objective** | Parameter validation and rejection |
| **Pass Criteria** | All invalid params rejected with correct codes; valid params committed atomically |
| **Prerequisites** | Param manager functional |
| **Hardware** | XDS100v3 |
| **Procedure** | Per P6c |
| **Evidence** | Reject count and reason code per test case, commit count increment |
| **Failure Criteria** | Invalid param accepted; valid param rejected; partial update observed |
| **Safety Action** | None (parameter-only test) |
| **Status** | PENDING |
| **Result** | — |

### P7: UART→SPI Baseline Regression

| Field | Value |
|-------|-------|
| **ID** | S4-P7-01 |
| **PRD Reference** | Step 1 §"JTAG下的1/64/65字节回归与改名前一致", Step 4 |
| **Dependencies** | S4-P1-02 |
| **Objective** | JTAG-loaded 1/64/65 byte regression with current binary |
| **Pass Criteria** | All counter deltas match historical baseline |
| **Prerequisites** | Prototype_RAM_Debug loaded |
| **Hardware** | XDS100v3, USB-to-UART, CPLD board |
| **Procedure** | Per P7a |
| **Evidence** | Counter audit trail table (before/after/delta/expected/match per test) |
| **Failure Criteria** | Any counter delta mismatch |
| **Safety Action** | Investigate regression; compare against Step 3 baseline |
| **Status** | PENDING |
| **Result** | — |

| Field | Value |
|-------|-------|
| **ID** | S4-P7-02 |
| **PRD Reference** | Step 4 |
| **Dependencies** | S4-P1-02, S4-P7-01 |
| **Objective** | Flash standalone 1/64/65 byte regression |
| **Pass Criteria** | Flash results match JTAG results |
| **Prerequisites** | Flash programmed, standalone boot confirmed |
| **Hardware** | XDS100v3 (hot-attach), USB-to-UART, CPLD board |
| **Procedure** | Per P7b |
| **Evidence** | Counter audit trail, comparison table (JTAG vs Flash) |
| **Failure Criteria** | Flash results differ from JTAG; any counter anomaly |
| **Safety Action** | Investigate Flash vs RAM behavior difference |
| **Status** | PENDING |
| **Result** | — |

### P8: Long-Run Stability

| Field | Value |
|-------|-------|
| **ID** | S4-P8-01 |
| **PRD Reference** | Step 4 §"8～24小时运行无死锁、内存踩踏和未解释错误计数增长" |
| **Dependencies** | S4-P1–P7 all PASS |
| **Objective** | 8-hour continuous run with periodic UART traffic, zero fault/overflow |
| **Pass Criteria** | No faults, no overflows, no deadlock, conservation laws hold, counters monotonic |
| **Prerequisites** | All prior phases complete |
| **Hardware** | Target board (Flash-programmed), USB-to-UART, bench supply, PC logger |
| **Procedure** | Per P8 |
| **Evidence** | Checkpoint logs (every 2h or at end), serial console log, final counter dump |
| **Failure Criteria** | FAULT, overflow, deadlock, conservation law violation >1% |
| **Safety Action** | Power off, save all logs before reset |
| **Status** | PENDING |
| **Result** | — |

| Field | Value |
|-------|-------|
| **ID** | S4-P8-02 |
| **PRD Reference** | Step 4 §"8～24小时运行" |
| **Dependencies** | S4-P8-01 |
| **Objective** | Extended 24-hour run (Industrial release candidate) |
| **Pass Criteria** | Same as S4-P8-01 but for 24 hours |
| **Prerequisites** | S4-P8-01 PASS |
| **Hardware** | Same as S4-P8-01 |
| **Procedure** | Per P8, extended to 24h |
| **Evidence** | Checkpoint logs every 4 hours |
| **Failure Criteria** | Same as S4-P8-01 |
| **Safety Action** | Power off, save all logs |
| **Status** | PENDING |
| **Result** | — |

### P9: Release Archival

| Field | Value |
|-------|-------|
| **ID** | S4-P9-01 |
| **PRD Reference** | Step 4 §"建立最终测试矩阵、发布清单和已知限制" |
| **Dependencies** | S4-P1–P8 |
| **Objective** | Produce final release package with all artifacts |
| **Pass Criteria** | All 4 configs build deterministically; all test records archived; release checklist signed |
| **Prerequisites** | All prior phases PASS |
| **Hardware** | PC with toolchain |
| **Procedure** | Per P9 |
| **Evidence** | Release directory tree, SHA-256 manifest, signed checklists |
| **Failure Criteria** | Non-deterministic build; missing evidence; checklist not complete |
| **Safety Action** | None |
| **Status** | PENDING |
| **Result** | — |

---

## 4. Measurement Plans

### 4.1 Flash Standalone Boot (S4-P1-02)

**Measurement method:**
1. Program Flash via CCS JTAG.
2. Disconnect JTAG physically (unplug XDS100v3 USB).
3. Power off bench supply. Wait 5 seconds for full discharge.
4. Power on bench supply. Wait 2 seconds.
5. Send test frame via serial console. Observe response.
6. Hot-attach JTAG, read diagnostic counters.
7. Repeat × 3.

**Instrument settings:** Bench supply at nominal DSP voltage (3.3V or 5V per board regulator). Current limit at 200 mA.

**Data to record:**
- Boot time (power-on to first UART response) — approximate from serial log timestamps
- Post-boot register snapshot (PLLSTS, SCICCR, SPICCR)
- Diagnostic counter values after 1-byte test frame
- Serial console log

### 4.2 PWM & ADC Waveforms

**PWM measurements (oscilloscope):**

| Measurement | Probe Points | Scope Settings | Expected Value | Tolerance |
|-------------|-------------|----------------|----------------|-----------|
| PWM frequency | EPWM1A (GPIO0) to GND | 2V/div, 5μs/div, trigger: rising edge | 60.00 kHz | ±1% |
| Dead-band RED | EPWM1A + EPWM1B to GND | 2V/div, 200ns/div, trigger: EPWM1A falling | 333 ns | ±20 ns |
| Dead-band FED | EPWM1A + EPWM1B to GND | 2V/div, 200ns/div, trigger: EPWM1B falling | 333 ns | ±20 ns |
| Duty cycle (safe passthrough) | EPWM1A to GND | 2V/div, 5μs/div | ~5.0% (50 permill) | ±1% |
| Disable response | EPWM1A to GND | 2V/div, 10μs/div, trigger: single-shot on falling | <1 PWM cycle | Must be <1 cycle |

**ADC measurement (JTAG + oscilloscope):**
- JTAG: read `diag->adc_raw[0..1]` for N=100 samples at known DC input
- Calculate: mean, stddev, min, max
- Oscilloscope: probe ADC input pin to confirm signal integrity

### 4.3 WCET Measurement

**Infrastructure:** CPU Timer2 at 150 MHz (6.67 ns/tick), 32-bit down-counter, free-running.

**Measurement points (already instrumented in code):**
- `isr.c:44-50`: Timer0 ISR (t0 - now)
- `isr.c:55-65,104-105`: SCI RX ISR (both early-return and normal paths)
- `isr.c:140,231-232`: ADC ISR (full path including Control_FastStep)
- `main.c:59-61`: Main loop iteration (prev - now)

**Procedure per WCET slot:**
1. Let system run for ≥60 seconds with representative load.
2. Pause via JTAG.
3. Read `WcetSlot` struct at known address:
   - `timer0_isr`: 0xC2C0 (min), 0xC2C2 (max), 0xC2C4 (last)
   - `sci_rx_isr`: 0xC2C6 (min), 0xC2C8 (max), 0xC2CA (last)
   - `adc_isr`: 0xC2CC (min), 0xC2CE (max), 0xC2D0 (last)
   - `main_loop`: 0xC2D2 (min), 0xC2D4 (max), 0xC2D6 (last)
4. Record all nine values.
5. Convert to microseconds: μs = cycles / 150.
6. Record sample count (from `adc_isr_count`, `timer0_isr` inferred from uptime).

**WCET budget analysis:**

| ISR | Period | Budget (20%) | Measured WCET | Margin |
|-----|--------|-------------|---------------|--------|
| ADC ISR | 16.67 μs (60 kHz) | 3.33 μs | TBD | TBD |
| Timer0 ISR | 100 μs (10 kHz) | 20 μs | TBD | TBD |
| SCI RX ISR | Event-driven | N/A | TBD | N/A |
| Main Loop | Background | N/A | TBD | N/A |

### 4.4 Trip Zone Measurement

**TZ1 hardware trip (oscilloscope):**

1. CH1: EPWM1A, CH2: TZ1 (GPIO12)
2. Trigger: CH2 falling edge, single-shot
3. Timebase: 100 ns/div (to capture trip latency)
4. Measurement: time from TZ1 falling edge crossing 1.5V to EPWM1A falling below 0.5V
5. Expected: <100 ns (combinatorial path through TZ module — no ISR involved)

**TZ latch verification (JTAG):**
- Read `EPwm1Regs.TZFLG.all` → OST1 bit must be set
- Read `diag->trip_flags` → must be non-zero
- Attempt `DrvEpwm_Enable(1)` → must return error (non-zero)

### 4.5 Long-Run Stability Monitoring

**Sampling interval:** Checkpoint every 2 hours (or at test end for 8h run).

**Log fields per checkpoint:**

| Field | Address/Source | Expected Behavior |
|-------|---------------|-------------------|
| `sci_rx_total` | diag +0x26 | Monotonic increasing at expected rate |
| `sci_rx_overflow` | diag +0x18 | 0 (no growth) |
| `miss_1ms` | diag +0x12 | 0 (no growth; occasional single-digit acceptable) |
| `miss_10ms` | diag +0x14 | 0 |
| `miss_100ms` | diag +0x16 | 0 |
| `fault_code` | diag +0x20 | 0 (FAULT_NONE) |
| `system_state` | diag +0x1E | 3 (RUN) |
| `param_reject_count` | diag +0x22 | 0 (no param submissions) |
| `telem_overrun_count` | diag +0x24 | 0 |
| `uart.rx_bytes` | g_app.spi_bridge.uart.rx_bytes | Monotonic |
| `uart.ready_frames` | g_app.spi_bridge.uart.ready_frames | ~2 × elapsed_seconds |
| `spi.req_bytes` | g_app.spi_bridge.spi.req_bytes | ≈ uart.rx_bytes − uart.busy_drops |
| `spi.timeouts` | g_app.spi_bridge.spi.timeouts | 0 |
| `frames_forwarded` | g_app.spi_bridge.frames_forwarded | = uart.ready_frames |

**Stop conditions (automatic detection via serial script):**
- JTAG readback shows `fault_code != 0`
- JTAG readback shows `sci_rx_overflow` increased since last checkpoint
- Serial console shows no response for >10 consecutive test frames

**Data retention:** All checkpoint data saved to dated log file. Raw JTAG memory reads preserved for post-hoc analysis.

---

## 5. Fault Injection Plan

### 5.1 Trip Zone (S4-P5-01)

**Injection method:** Pull TZ1 (GPIO12) LOW via jumper wire to GND.
**Expected state transition:** RUN → FAULT (via `System_EnterFault(FAULT_HW_TZ_TRIP)` in ADC ISR).
**PWM response:** Hardware (nanoseconds): TZ module forces EPWMxA/B LOW. Software (microseconds): `pwm_disable_requested = 1`, consumed at 1ms task → `DrvEpwm_Disable()`.
**Diagnostic evidence:** TZFLG.OST1=1, `diag->trip_flags != 0`, `diag->fault_code = FAULT_HW_TZ_TRIP`, `diag->system_state = FAULT`.
**Recovery:** Remove jumper (TZ1 returns HIGH). Call `DrvEpwm_ClearOstTrip(1)`. Call `System_ClearFault()` (Prototype only). Verify STANDBY state. Call `DrvEpwm_Enable(1)`.

### 5.2 ADC Stuck-High / Stuck-Low (S4-P5-02)

**Injection method (JTAG breakpoint):** Set breakpoint in ADC ISR at `Control_FastStep()` return. Modify `ctrl_output.fault_asserted = 1`, `ctrl_output.fault_code = 1` (stuck-high) or `2` (stuck-low).
**Expected state transition:** RUN → FAULT via `FAULT_SW_CONTROL_INVALID + fault_code`.
**PWM response:** `pwm_disable_requested = 1` → `DrvEpwm_Disable()` at 1ms.
**Diagnostic evidence:** Fault code latched, PWM output LOW (scope).
**Recovery:** `System_ClearFault()` (Prototype).

### 5.3 SPI Timeout (S4-P6-01)

**Injection method:** Physically disconnect MISO (GPIO17) from CPLD.
**Expected behavior:** SPI byte transfer starts (TXBUF written) but `SpiRequest_Service()` times out after 5 ms. `spi.timeouts` increments each timed-out byte. After cumulative timeouts exceed `fault_thresh_spi_timeout` (checked at 100ms), `FAULT_COMM_SPI_TIMEOUT_EXCESSIVE` asserted.
**PWM response:** HW_CONFIRMED=0 → PWM already disabled. If HW_CONFIRMED=1, PWM disabled via `DrvEpwm_Disable()`.
**Diagnostic evidence:** `spi.timeouts > 0`, `fault_code = FAULT_COMM_SPI_TIMEOUT_EXCESSIVE`.
**Recovery:** Reconnect MISO. Clear fault (Prototype) or power-cycle (Industrial).

### 5.4 MISO Anomaly (S4-P6-01)

**Injection method:** If CPLD can be configured to hold MISO at 0xFF (idle) or 0x00, toggle between modes.
**Expected behavior:** MISO=0xFF increments `miso_idle_ff`. MISO≠0xFF increments `miso_unexpected`. Neither causes fault by itself (diagnostic counters only).
**PWM response:** No PWM response (communication-only anomaly).
**Diagnostic evidence:** Counter values reflect CPLD response state.

### 5.5 Parameter Rejection (S4-P6-02)

**Injection method:** Via CCS Expressions, write invalid values to `g_app.param_manager.pending.*`, then set `commit_requested = 1`.
**Expected behavior:** `Param_CheckPendingCommit()` at 1ms task validates and rejects. `reject_count++`, `last_reject_reason` set. `active` NOT modified.
**PWM response:** No PWM response (parameter-only).
**Diagnostic evidence:** Reject counter increments, reason code correct, active params unchanged.
**Note:** After cumulative rejections, `FAULT_SYS_PARAM_REJECTED` may be asserted if implemented (check `state_machine.c` for threshold).

### 5.6 Communication Congestion / Queue Overflow (S4-P8)

**Injection method:** Send UART frames at maximum rate with no inter-frame gap (continuous burst at 9600 bps).
**Expected behavior:** `uart.busy_drops` increases (single-frame buffer policy). `overflow_count` remains 0 (129-slot SPSC queue not exceeded at 9600 bps). No crash or hang.
**PWM response:** No PWM response (HW_CONFIRMED=0).
**Diagnostic evidence:** `busy_drops > 0`, `overflow_count = 0`, no fault.

### 5.7 Watchdog Strategy — GAP

**PRD Requirement** (FR-11 Industrial): "Watchdog策略启用并经过故障注入验证"

**Current status:** Watchdog is NOT implemented in the current firmware. The `WDCR` and `WDKEY` registers are not configured. No watchdog service routine exists.

**Gap:** This is a known gap. The Industrial watchdog requirement cannot be verified in Step 4.

**Plan:** Watchdog implementation is deferred to a future step. The release checklist for Industrial_Flash_Release must explicitly note this as a known limitation.

**Mitigation:** The existing safety layers (AQCSFRC force LOW, TBCLKSYNC=0, TZ hardware trip, software fault paths) provide hardware-enforced PWM safety even without a watchdog. A watchdog would add CPU-lockup protection but is not required for the current platform baseline (UART→SPI bridge reference application with no power stage connected).

---

## 6. Release & Traceability Plan

### 6.1 Prototype Demo Release Checklist

All items must be checked before a Prototype demo binary is distributed.

- [ ] **Build:** Prototype_Flash_Demo, 0 errors, 0 warnings
- [ ] **Identity:** `PLATFORM_BUILD_ID` = "Prototype_Flash_Demo"
- [ ] **Label:** Binary filename includes "Prototype" and is NOT named with "Industrial" or "Release"
- [ ] **PWM:** `BOARD_PWM_ADC_HW_CONFIRMED = 0U` (PWM permanently disabled)
- [ ] **Duty clamp:** `BOARD_PWM_MAX_DUTY_PERMILL = 480` (48.0% hard limit)
- [ ] **TZ:** Trip Zone configured, one-shot enabled
- [ ] **Fault:** Fault recovery enabled (Prototype quick-reset)
- [ ] **Debug:** JTAG halt allowed, extended diagnostics active
- [ ] **Regression:** Host tests (all suites) PASS
- [ ] **Regression:** UART→SPI 1/64/65 byte regression PASS
- [ ] **Documentation:** KNOWN_LIMITATIONS.md included with binary
- [ ] **Non-production:** Binary metadata clearly states "NOT FOR PRODUCTION"

### 6.2 Industrial Release Checklist

All items must be checked before an Industrial release is tagged.

- [ ] **Build:** All 4 configurations, 0 errors, 0 warnings
- [ ] **Identity:** `PLATFORM_BUILD_ID` = "Industrial_Flash_Release"
- [ ] **Static checks:** All boundary checks PASS
- [ ] **Host tests:** All suites (Prototype + Industrial) PASS
- [ ] **Flash boot:** 3/3 standalone power-cycle boots PASS
- [ ] **PWM default-off:** Confirmed in all states (S4-P2-01 PASS)
- [ ] **PWM waveforms:** Frequency, DB, duty cycle verified (S4-P2-02 PASS)
- [ ] **ADC sampling:** SOCA timing confirmed (S4-P3-01 PASS)
- [ ] **WCET:** All ISRs within budget (S4-P4-01–03 PASS)
- [ ] **Trip Zone:** TZ1/TZ2 hardware trip verified (S4-P5-01 PASS)
- [ ] **Fault paths:** All software fault paths → PWM disable (S4-P5-02 PASS)
- [ ] **Fault lock:** Industrial FAULT lock verified (S4-P5-03 PASS)
- [ ] **SPI timeout:** Detection + fault escalation verified (S4-P6-01 PASS)
- [ ] **Parameter reject:** All invalid combos rejected (S4-P6-02 PASS)
- [ ] **Regression:** 1/64/65 byte Flash standalone regression PASS (S4-P7-02 PASS)
- [ ] **Long-run:** 8h minimum, 24h preferred, zero faults (S4-P8 PASS)
- [ ] **Config CRC:** `PLATFORM_CONFIG_CRC` computed and embedded
- [ ] **Git tag:** `v1.0.0` signed tag created
- [ ] **SHA-256 manifest:** All .out, .map, .hex files hashed
- [ ] **Release archive:** Complete per P9 directory structure
- [ ] **Known limitations:** Documented gaps explicitly (Watchdog, LED callers, etc.)
- [ ] **Reproducibility:** Clean checkout → build → identical .out

### 6.3 Required Archived Artifacts

| Artifact | Format | Purpose |
|----------|--------|---------|
| Source code | Git tag `v<X>.<Y>.<Z>` | Exact source state |
| Build command | Text (script + args) | Reproduce build |
| Compiler version | `cl2000 --version` output | Toolchain identity |
| `.out` files (4 configs) | ELF binary | Executable |
| `.map` files (4 configs) | Text | Memory layout, symbols |
| `.hex` files (if generated) | Intel HEX | Alternative programming format |
| Build logs (4 configs) | Text | Compiler/linker output |
| Host test logs | Text | Test pass/fail evidence |
| Quality gate output | Text | 8-gate result |
| Test records (P0–P9) | Markdown | Per-phase evidence |
| WCET records | CSV | Min/max/last, sample count |
| Oscilloscope screenshots | PNG | Waveform evidence |
| SHA-256 manifest | Text | Integrity verification |
| KNOWN_LIMITATIONS.md | Markdown | Gaps and caveats |

### 6.4 Git Tag Naming Convention (Plan Only)

```
v<MAJOR>.<MINOR>.<PATCH>[-<pre-release>]

Examples:
  v1.0.0           — First production release (Industrial_Flash_Release)
  v1.0.0-rc1       — Release candidate 1
  v0.9.0-demo      — Prototype demo release
```

Tags must be annotated (`git tag -a`) with a message containing:
- Platform version string
- Build identity
- Git commit SHA (redundant but explicit)
- Release date
- Brief change summary

### 6.5 Release Version Naming Convention

The version is maintained in `firmware/platform_profile.h`:
- `PLATFORM_VERSION_MAJOR` — Increment for breaking API/architecture changes
- `PLATFORM_VERSION_MINOR` — Increment for new features (new drivers, algorithms)
- `PLATFORM_VERSION_PATCH` — Increment for bug fixes and documentation updates

Current: `1.0.0` (initial development baseline).

Recommendation for Step 4 completion:
- First Industrial release: `v1.0.0`
- Prototype demo prior: `v0.9.0-demo`

---

## 7. PRD Step 4 Acceptance Gate Mapping

| PRD Step 4 Acceptance Gate | Test ID(s) | Status |
|---------------------------|------------|--------|
| Flash断电独立启动通过 | S4-P1-01, S4-P1-02, S4-P1-03 | PENDING |
| 所有关键ISR有真实WCET和裕量记录 | S4-P4-01, S4-P4-02, S4-P4-03 | PENDING |
| PWM、ADC触发、控制更新和Trip波形符合设计 | S4-P2-02, S4-P3-01, S4-P5-01 | PENDING |
| 故障注入结果与安全策略一致 | S4-P5-01–03, S4-P6-01–02 | PENDING |
| 8～24小时运行无死锁、内存踩踏和未解释错误计数增长 | S4-P8-01, S4-P8-02 | PENDING |
| 新工程师可依据README和文档完成构建、烧写、测试和问题定位 | S4-P9-01 (documentation review) | PENDING |
| 发布标签明确区分平台版本与具体产品版本 | S4-P9-01 (tag naming review) | PENDING |
| Industrial_Flash_Release满足全部工业验收门 | All P1–P9 | PENDING |
| Prototype产物带有清晰的非生产标识 | S4-P9-01 (Prototype checklist) | PENDING |

---

## 8. Known Gaps & Deferred Items

| # | Item | Reason | Impact | Mitigation |
|---|------|--------|--------|------------|
| G1 | Watchdog not implemented | FR-11 requirement, not yet coded | Industrial cannot meet full safety spec | Document in KNOWN_LIMITATIONS; deferred to future step |
| G2 | Parameter Flash persistence | FR-08 mention, not in Step 3 scope | Parameters lost on power cycle | Acceptable for Step 4 baseline; deferred |
| G3 | Bootloader / field upgrade | Non-goal per PRD §6 | Flash programming requires JTAG | Not required for Step 4 |
| G4 | LED callers not wired | Known architecture gap | TX/RX LEDs don't illuminate | Document; not a safety issue |
| G5 | CAN/I2C not implemented | PRD §6 non-goal | Communication limited to SCI+SPI | Not required for Step 4 |
| G6 | Additional control algorithms (PID, PR, Droop) | PRD §6 non-goal | Only SafeOpenLoop exists | Not required for Step 4 |
| G7 | DMA ADC sampling | FR-02 mention, deferred | CPU-driven ADC sampling only | Not required for Step 4 |
| G8 | Platform template vs. product split | Step 4 exploratory item | Single repo for both | Evaluation deferred post-Step 4 |
| G9 | Prototype_Flash_Demo + Industrial_RAM_Debug build gate | Step 1 PENDING (low priority) | Only 2/4 configs in quality gate | Extend quality gate when convenient |

---

## Appendix A: Document References

| Document | Path | Role in Step 4 |
|----------|------|---------------|
| PRD | [PRD_F28335_RTCONTROL_PLATFORM.md](PRD_F28335_RTCONTROL_PLATFORM.md) | Normative requirements |
| Step 1 Report | [ACCEPTANCE_REPORT_STEP1.md](ACCEPTANCE_REPORT_STEP1.md) | Baseline acceptance status |
| Step 2 Design | [DESIGN_STEP2_ADC_PWM_TZ.md](DESIGN_STEP2_ADC_PWM_TZ.md) | ADC/PWM/TZ design reference |
| Step 2 Evidence | [EVIDENCE_STEP2_BUILD_VERIFICATION.md](EVIDENCE_STEP2_BUILD_VERIFICATION.md) | Build + static verification |
| Step 3 Design | [DESIGN_STEP3_CONTROL_STATE_PARAMS_TELEMETRY.md](DESIGN_STEP3_CONTROL_STATE_PARAMS_TELEMETRY.md) | Control/state/params design |
| Step 3 Evidence | [EVIDENCE_STEP3_SOFTWARE_VERIFICATION.md](EVIDENCE_STEP3_SOFTWARE_VERIFICATION.md) | Software verification |
| Build Guide | [BUILD_AND_FLASH.md](BUILD_AND_FLASH.md) | Build procedures, Flash programming |
| Hardware Test | [HARDWARE_TEST.md](HARDWARE_TEST.md) | Test procedures, register baselines |
| Architecture | [ARCHITECTURE.md](ARCHITECTURE.md) | Layer model, data ownership |
| Memory Layout | [MEMORY_LAYOUT.md](MEMORY_LAYOUT.md) | Section placement, addresses |
| Task Timing | [TASK_TIMING.md](TASK_TIMING.md) | WCET infrastructure, timing constants |
| Baseline Record | [BASELINE_TEST_RECORD.md](../tests/hardware/BASELINE_TEST_RECORD.md) | Historical regression baseline |
| Platform Profile | [platform_profile.h](../firmware/platform_profile.h) | Build identity, capability macros |
| Board Config | [board_config.h](../firmware/bsp/board_config.h) | HW_CONFIRMED gate, pin mapping |

---

## Appendix B: Hardware / Instrument Checklist

| Item | Required For | Minimum Spec |
|------|-------------|-------------|
| XDS100v3 JTAG probe | All phases | TI XDS100v3 or compatible |
| TMS320F28335 target board | All phases | Custom board or TI controlCARD |
| USB-to-UART adapter (FTDI) | P1, P4b, P6, P7, P8 | 3.3V TTL, 9600 bps minimum |
| Oscilloscope | P2, P3, P5 | ≥100 MHz bandwidth, ≥2 channels |
| Oscilloscope probes | P2, P3, P5 | 10×, ≥100 MHz, properly compensated |
| Function generator | P3b | DC to 1 kHz sine, 0–3V output |
| DC power supply (bench) | All | Adjustable, current-limited, for DSP board |
| Multimeter | P0 | Continuity, DC voltage |
| Jumper wires | P5 | Male-to-male, for TZ pin toggling |
| 10 kΩ resistor | P5 | Pull-up for TZ pin |
| PC with CCS 20.5.1+ | All | Windows, CCS with C2000 CGT 25.11.0.LTS |
| PC with MSVC 2022 | P9 (host tests) | Visual Studio 2022 Community or higher |
| Serial logging PC | P8 | Any OS with serial terminal + logging |
