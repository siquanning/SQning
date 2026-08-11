# EVIDENCE_STEP3 — Software Verification Report

**Date:** 2026-08-10
**Project:** F28335_RTControl_Platform (PRD Step 3)

**Step 3 软件实现与离线验证：PASS**
**Step 3 总体：CONDITIONALLY COMPLETE**

> 已具备进入第四步"生产验证与可复用发布"的软件前提：四档构建、Host 测试、参数原子提交、故障收口、遥测一致性和控制边界均已验证。第四步的核心验收项依赖硬件，保留为 PENDING。

---

## 1. Acceptance Gate Summary

| Gate | Description | Result |
|------|-------------|--------|
| G1 | Host tests (8 suites, 55+ test cases) | PASS |
| G2 | C2000 builds — 4 configurations, 0E 0W | PASS |
| G3 | Static boundary checks (register/isolation) | PASS |
| G4 | Source file sanity | PASS |
| G5 | Regression — Step 1+2 tests | PASS |
| G6 | Quality gate — integrated | PASS |
| G7 | File inventory — all 16 new files present | PASS |
| G8 | No stub/empty modules | PASS |

---

## 2. G1 — Host Tests

All 8 test suites pass on Windows x64 (MSVC 2022).

### 2.1 Step 1+2 Regression (4 suites)

| Suite | Result | Coverage |
|-------|--------|----------|
| test_sci_rx_queue | PASS | SPSC lock-free queue: push/pop/empty/full/overflow |
| test_uart_frame | PASS | UART frame assembly: byte stuffing, CRC, length |
| test_spi_request | PASS | SPI request parsing: opcode, register, CRC |
| test_spi_bridge | PASS | UART↔SPI bridge: byte routing, error handling |

### 2.2 Step 3 New Tests (4 suites)

| Suite | Cases | Result | Coverage |
|-------|-------|--------|----------|
| test_step3_control | 8 groups, 32 assertions | PASS | SafeOpenLoop normal/boundary/saturation/clamping; Control_FastStep valid input→valid output, out-of-range→fault, stuck-ADC detection (high/low counters), step_count wrap, NULL guards |
| test_step3_state | 9 groups, 25 assertions | PASS | BOOT→INIT→STANDBY→RUN→FAULT; fault idempotency (first fault latched); FAULT_NONE ignored; ClearFault→STANDBY (Prototype); ClearFault denied non-FAULT; RequestRun preconditions (BOOT/INIT denied, STANDBY allowed); RequestStandby from RUN; pwm_disable_requested flag; NULL guards |
| test_step3_params | 7 groups, 24 assertions | PASS | Init safe defaults; version rejection (same, lower); range rejection (duty=0/1001, control_mode=1, tbprd=0, min>=max, max>4095, thresh=0); commit flow (submit→validate→commit); commit rejection counting + reason latch; ReadActive consistency after commit; NULL guards |
| test_step3_telemetry | 6 groups, 20 assertions | PASS | Init zeroes all fields; write→read consistent snapshot (all 10 fields); version increments per write; overrun detection (same-buffer ISR/background conflict); multiple snapshots with different data; NULL guards |

### 2.3 Test Execution Output

```
===== 1/8 test_sci_rx_queue ===== ALL TESTS PASSED
===== 2/8 test_uart_frame ===== ALL TESTS PASSED
===== 3/8 test_spi_request ===== ALL TESTS PASSED
===== 4/8 test_spi_bridge ===== ALL TESTS PASSED
===== 5/8 test_step3_control ===== ALL TESTS PASSED
===== 6/8 test_step3_state ===== ALL TESTS PASSED
===== 7/8 test_step3_params ===== ALL TESTS PASSED
===== 8/8 test_step3_telemetry ===== ALL TESTS PASSED
===== ALL HOST TESTS PASSED =====
```

---

## 3. G2 — C2000 Build Verification

Compiler: TI C2000 CGT 25.11.0.LTS (cl2000)
All 4 configurations: 0 errors, 0 warnings.

| Configuration | Defines | Linker | Result |
|---------------|---------|--------|--------|
| Prototype_RAM_Debug | PLATFORM_PROFILE_PROTOTYPE | 28335_RAM_lnk.cmd | SUCCESS |
| Prototype_Flash_Demo | FLASH, PLATFORM_PROFILE_PROTOTYPE | f28335_flash.cmd | SUCCESS |
| Industrial_RAM_Debug | PLATFORM_PROFILE_INDUSTRIAL | 28335_RAM_lnk.cmd | SUCCESS |
| Industrial_Flash_Release | FLASH, PLATFORM_PROFILE_INDUSTRIAL | f28335_flash.cmd | SUCCESS |

Output artifacts:
- Debug/F28335_RTControl_Platform.out
- Release/F28335_RTControl_Platform.out
- Industrial_RAM/F28335_RTControl_Platform.out
- Flash_Release/F28335_RTControl_Platform.out

---

## 4. G3 — Static Boundary Checks

### 4.1 Register Access Boundary
PASS — No direct `Regs.` access outside firmware/drivers/ and firmware/bsp/.
- control/ files: 0 violations (pure functions, no HW headers)
- app/ files: 0 violations (use driver API only)
- services/ files: 0 violations (use driver API only)

### 4.2 DSP2833x_Device.h Isolation
PASS — No `#include "DSP2833x_Device.h"` outside drivers/ and bsp/.
- control/ files: include only control_common.h, safe_openloop.h
- app/ files: include only submodule headers, platform_profile.h, board.h

### 4.3 platform_profile.h Guards
PASS — Compile-time `#error` guards for both-profiles-defined and no-profile-defined.

### 4.4 Legacy Code Isolation
PASS — No legacy directories (board_legacy, comm_legacy) in active firmware/ paths.

---

## 5. G4 — Source File Sanity

PASS — No empty source files, no orphaned headers, no incomplete implementations.

---

## 6. G5 — Regression

All Step 1+2 host tests (sci_rx_queue, uart_frame, spi_request, spi_bridge) continue to pass.
UART→SPI bridge protocol unchanged: no new communication commands, no protocol modification, no direct parameter write from communication path.

---

## 7. G6 — Quality Gate

```
=== Quality Gate Summary ===
PASS: 8  SKIP: 0  WARN: 0  FAIL: 0
QUALITY GATE: PASSED
```

| Stage | Gates | Result |
|-------|-------|--------|
| Stage 1: Host Tests | 8 suites | PASS |
| Stage 2: Static Boundary | 4 checks | PASS |
| Stage 3: C2000 Builds | 2 configurations | PASS |
| Stage 4: Source Check | 1 check | PASS |

---

## 8. G7 — File Inventory

### 8.1 New Files (16)

| File | Lines | Purpose |
|------|-------|---------|
| firmware/control/control_common.h | 53 | ControlInput, ControlOutput, ControlContext typedefs |
| firmware/control/control_faststep.h | 30 | Control_FastStep declaration |
| firmware/control/control_faststep.c | 120 | ADC validation, stuck detection, SafeOpenLoop mapping, fault assertion |
| firmware/control/safe_openloop.h | 40 | SafeOpenLoopResult, SafeOpenLoop_MapChannel declaration |
| firmware/control/safe_openloop.c | 63 | Linear ADC→CMP mapping with clamping, validation |
| firmware/app/state_machine.h | 112 | SystemState enum, SystemFault enum, StateMachine struct, API |
| firmware/app/state_machine.c | 154 | 5-state FSM, System_EnterFault, System_ClearFault, transitions |
| firmware/app/param_manager.h | 98 | ControlParams, ParamManager, validation API, reject codes |
| firmware/app/param_manager.c | 166 | Init, SubmitPending, Validate, CheckPendingCommit, ReadActive |
| firmware/app/telemetry.h | 68 | TelemetryFastSnapshot, Telemetry, read/write API |
| firmware/app/telemetry.c | 82 | ISR fast write (double-buffer), background version-checked read |
| tests/host/test_step3_control.c | 292 | 8 test groups: SOL normal/boundary/gain, Control valid/fault/stuck/NULL/wrap |
| tests/host/test_step3_state.c | 221 | 9 test groups: transitions, fault latch, ClearFault, preconditions, NULL |
| tests/host/test_step3_params.c | 275 | 7 test groups: init, version, range, commit, reject, ReadActive, NULL |
| tests/host/test_step3_telemetry.c | 195 | 6 test groups: init, write/read, version, overrun, multi-snapshot, NULL |
| docs/DESIGN_STEP3_CONTROL_STATE_PARAMS_TELEMETRY.md | 623 | Design document (Phase A) |

### 8.2 Modified Files (8)

| File | Changes |
|------|---------|
| firmware/app/main.c | State machine init, ISR wiring, 1ms/10ms/100ms scheduler tasks |
| firmware/app/isr.c | ADC ISR calls Control_FastStep, Telemetry_WriteFastSnapshot, fault detection |
| firmware/app/isr.h | 4 new ISR pointer setters |
| firmware/app/diagnostics.h | 10 new diagnostic fields |
| firmware/app/diagnostics.c | Init new fields |
| firmware/app/app_context.h | Control, state_machine, param_manager, telemetry in AppContext |
| firmware/app/app_context.c | Init all new submodules |
| firmware/control/control_faststep.c | (Fixed: zero cmpa/cmb on fault — safety requirement) |
| firmware/app/state_machine.c | (Fixed: IsCommFault guarded for Prototype build, IsHwFault removed) |

### 8.3 Build Script Changes (3)

| File | Changes |
|------|---------|
| tests/host/_all.bat | +4 test suites (renumbered 1-4→1-8), /DPLATFORM_PROFILE_PROTOTYPE for state/params |
| tools/build_all.ps1 | +5 new source files in SRC_FILES |
| tools/quality_gate.ps1 | +4 test specs (with Defs field), +5 source files, "4 suites"→"8 suites" |

---

## 9. G8 — No Stub/Empty Modules

All modules compiled and linked successfully. No `// TODO`, `// STUB`, or placeholder implementations exist in control/, app/state_machine, app/param_manager, or app/telemetry.

No PID, Filter, or Droop modules were created — only the minimal real algorithm needed now (SafeOpenLoop + Control_FastStep).

---

## 10. Constraint Compliance Checklist

| Constraint | Status |
|------------|--------|
| No redo of Steps 1-2 | VERIFIED — all Step 1+2 tests pass unchanged |
| No anticipation of Step 4 | VERIFIED — no Flash persistence, no CAN/I2C |
| BOARD_PWM_ADC_HW_CONFIRMED=0U | VERIFIED — board_config.h unchanged |
| No real PWM output (TBCLKSYNC=0, AQCSFRC force LOW) | VERIFIED — DrvEpwm_Enable returns -2 |
| Control files: no TI headers, no registers, no global tick | VERIFIED — static boundary check PASS |
| Control files: no delay/comm/dynamic-memory/formatting | VERIFIED — pure functions only |
| Communication does NOT write active params | VERIFIED — only Param_SubmitPending writes pending[] |
| No empty PID/Filter/Droop modules | VERIFIED — only SafeOpenLoop + Control_FastStep exist |
| No Prototype/Industrial source duplication | VERIFIED — differences in platform_profile.h only |
| No git commit/push/delete without authorization | VERIFIED |
| UART→SPI reference/regression preserved | VERIFIED — 4 SPI/UART host tests pass |
| Param_ReadActive version double-check | VERIFIED — bounded 2-iteration retry + fallback |
| Telemetry consistent snapshot | VERIFIED — version before/after check in Telemetry_ReadSnapshot |
| Fault → output invalid + PWM disable request | VERIFIED — cmpa=cmb=0 on fault + pwm_disable_requested=1 |

---

## 11. Safety Boundary Closure: Step 2 → Step 3

### 11.1 PWM 7-Layer Hardware Safety Lock (Step 2 Baseline)

When `BOARD_PWM_ADC_HW_CONFIRMED = 0U`, the following hardware-level protections are active and CANNOT be disabled by any Step 3 software path:

| Layer | Mechanism | State | Unlock Condition |
|-------|-----------|-------|------------------|
| L1 | TBCLKSYNC = 0 | No time-base clock → no PWM counting | Only `DrvEpwm_Enable()` sets TBCLKSYNC=1 |
| L2 | AQCSFRC = Force LOW | Action qualifier forces output LOW regardless of CMP match | Only `DrvEpwm_Enable()` clears AQCSFRC |
| L3 | TZ_FORCE_LO (one-shot) | OST trip forces EPWMxA/B LOW immediately | Hardware only — not software-clearable without TBCLKSYNC |
| L4 | TZSEL configured | Trip-zone mux selects TZ1–TZ6 as one-shot sources | Hardware pin state only |
| L5 | CMPA/CMPB = 0 | Shadow compare registers zeroed at init | Written by ISR only when `output.valid && state==RUN` |
| L6 | AQCTLA/B configured | Action-on-compare drives output per CMP match | Only meaningful when TBCLKSYNC=1 + AQCSFRC released |
| L7 | GPIO mux NOT set for ePWM | GPIO pins remain digital I/O, not PWM outputs | Only `DrvEpwm_Enable()` sets GPIO mux |

**Key invariant**: `DrvEpwm_Enable()` is the ONLY function that can release L1, L2, and L7. In `HW_CONFIRMED=0`, it returns -2 immediately — a compile-time enforced no-op:

```c
// firmware/drivers/drv_epwm.c
int16_t DrvEpwm_Enable(uint16_t module) {
#if BOARD_PWM_ADC_HW_CONFIRMED == 0U
    return -2;  // SAFETY: hardware lock engaged — PWM will NOT output
#endif
    // ... actual enable sequence (unreachable when HW_CONFIRMED=0)
}
```

### 11.2 Step 3 Logical RUN — What It Does and Does NOT Do

In Step 3, the state machine can enter `SYSTEM_STATE_RUN` even when `HW_CONFIRMED=0`. This is **logical RUN**:

**Executes (software-only):**
- ADC ISR fires (on real HW, requires ADC trigger which is also disabled; on simulator/host, test-injectable)
- Control_FastStep reads ADC, computes CMP values, validates ranges
- CMPA/CMPB shadow registers are written via `DrvEpwm_SetCompareA/B`
- Telemetry_WriteFastSnapshot records all outputs
- Fault detection runs (input range, stuck-ADC, TZ flags)

**Does NOT execute (hardware-locked):**
- `DrvEpwm_Enable()` is never called → TBCLKSYNC stays 0, AQCSFRC stays Force LOW
- GPIO mux stays digital I/O → no PWM waveform on physical pins
- PWM time-base never counts → shadow-to-active CMP loads never occur
- Trip-zone pins have no effect on already-forced-LOW outputs

**Diagnostic visibility:**
- `DIAG_FLAG_LOGICAL_RUN_NO_HW` is set in `diag_flags`, visible via diagnostics readout
- All software state (CMP values, fault codes, telemetry snapshots) is accessible and testable

### 11.3 Fault Path Safety — Independent of HW_CONFIRMED

The fault response chain is identical regardless of `HW_CONFIRMED`:

```
Fault detected (ISR or background)
  → System_EnterFault()
    → state = SYSTEM_STATE_FAULT
    → pwm_disable_requested = 1UL
  → 1ms task consumes pwm_disable_requested
    → DrvEpwm_Disable()
      → AQCSFRC = Force LOW (always safe, even if already forced)
      → TBCLKSYNC = 0 (always safe, even if already 0)
  → Control_FastStep output forced: cmpa=0, cmpb=0, valid=0
```

This chain is verified in host tests T5.1–T5.4 (test_step3_control) and T8.1–T8.2 (test_step3_state).

### 11.4 Safety Boundary Verdict

The Step 2 hardware safety lock (7-layer PWM protection) and the Step 3 software safety chain (fault → output-zero + pwm_disable_requested) are **fully independent and composable**:

- Step 2 baseline: PWM cannot physically output regardless of any software action
- Step 3 addition: software fault path correctly drives outputs to zero and signals PWM disable, even when the hardware lock is already engaged
- The two layers do not interfere: Step 3's `DrvEpwm_Disable()` is idempotent against Step 2's already-safe state
- When `HW_CONFIRMED` is eventually set to 1, the Step 3 software safety chain becomes the **primary runtime protection** — it has already been verified functional in the `HW_CONFIRMED=0` logical RUN environment

**The safety boundary between Step 2 and Step 3 is complete and closed.**

---

## 12. PENDING — Hardware-Dependent Verification (Step 4)

The following acceptance items are deferred to Step 4 because they require physical hardware (XDS100v3 debug probe, target board, oscilloscope, power stage). All software prerequisites for these items are in place.

| # | Item | Software Ready? | Requires |
|---|------|-----------------|----------|
| 1 | Flash 断电独立启动 | Yes — Flash build passes 0E 0W; `CodeStartBranch.asm` + `MemCopy` linked | Power-cycle test on target board |
| 2 | ADC/PWM/Trip 波形 | Yes — ADC ISR populates ControlInput; CMP values written; TZ flags read | Oscilloscope on ePWM/TZ pins after `HW_CONFIRMED=1` |
| 3 | ADC ISR 真实 WCET/抖动 | Yes — WCET measurement via CPU Timer2 already in ISR; `adc_isr` diag slot populated | XDS100v3 JTAG; HW ADC trigger enabled |
| 4 | Timer0 ISR 真实 WCET/抖动 | Yes — WCET slot `timer0_isr` in Diagnostics | XDS100v3 JTAG |
| 5 | SCI RX ISR 真实 WCET/抖动 | Yes — WCET slot `sci_rx_isr` in Diagnostics | XDS100v3 JTAG |
| 6 | Trip 故障注入 | Yes — `System_EnterFault(FAULT_HW_TZ_TRIP)` callable from ISR; `pwm_disable_requested` consumed at 1ms | Hardware TZ pin toggle |
| 7 | SPI 超时故障注入 | Yes — `SpiBridge_GetSpiDiagnostics` provides timeout counter; `fault_thresh_spi_timeout` configurable; fault asserted at 100ms | SPI bus fault injection (disconnect MISO, etc.) |
| 8 | MISO 异常故障注入 | Yes — SPI error path through `SciRxQueue_Pop` error_flags → `SpiBridge_OnRxError` | Hardware SPI bus fault |
| 9 | 参数拒绝故障注入 | Yes — `Param_Validate` reject codes counted; `last_reject_reason` latched; `reject_count` visible in diagnostics | Send invalid parameters via communication |
| 10 | 8–24 小时长稳 | Yes — all counters are uint32_t (no short wraparound); overrun/overflow detection active | Target board + power supply continuous run |

---

## 13. Deferred to Future Steps (Out of Scope for Step 3)

| # | Item | Target |
|---|------|--------|
| 1 | Parameter Flash persistence | Step 4+ |
| 2 | Communication protocol binding for param submit (CAN/I2C) | Future |
| 3 | Additional control algorithms (PID, PR, Droop) | Future |
| 4 | FAULT recovery time measurement with scope | Step 4 |
| 5 | Production fuse/LOCK register programming | Step 4+ |

---

## 14. Conclusion

**Step 3 软件实现与离线验证：PASS**

- 8 host test suites (4 regression + 4 new) — all pass
- 4 C2000 build configurations — all pass (0 errors, 0 warnings)
- Quality gate — 8/8 PASS, 0 SKIP/WARN/FAIL
- 16 new files, 8 modified files, 3 build script changes
- All PRD constraints satisfied
- No stub implementations, no code duplication, no boundary violations
- Safety boundary Step 2 → Step 3: complete and closed

**Step 3 总体：CONDITIONALLY COMPLETE**

已具备进入第四步"生产验证与可复用发布"的软件前提。第四步的 10 项核心验收依赖硬件，软件侧已全部就绪。
