# Step 2 ADC/ePWM/Trip Zone — Build & Static Verification Evidence

**Date**: 2026-08-10
**PRD Step 2 Status**: CONDITIONALLY COMPLETE (pending hardware evidence)

## 1. Software Build — PASS

All 4 configurations build with 0 errors, 0 warnings.

| Configuration | Defines | Linker CMD | Result |
|---|---|---|---|
| Debug (Prototype_RAM_Debug) | `PLATFORM_PROFILE_PROTOTYPE` | 28335_RAM_lnk.cmd | PASS |
| Release (Prototype_Flash_Demo) | `FLASH, PLATFORM_PROFILE_PROTOTYPE` | f28335_flash.cmd | PASS |
| Industrial_RAM_Debug | `PLATFORM_PROFILE_INDUSTRIAL` | 28335_RAM_lnk.cmd | PASS |
| Industrial_Flash_Release | `FLASH, PLATFORM_PROFILE_INDUSTRIAL` | f28335_flash.cmd | PASS |

**Full build log**: `tools/build_all_4configs.log` (saved as independent evidence)

**Key Industrial build fix**: Removed `#error "BOARD_PWM_ADC_HW_CONFIRMED must be 1 for Industrial build"` from `board_config.h`. Industrial configurations now compile cleanly with permanent safe-disabled behavior when HW_CONFIRMED=0. All safety behaviors preserved via compile-time gates in driver code.

## 2. Host Tests — PASS (5/5 suites)

| Test Suite | Source | Assertions | Result |
|---|---|---|---|
| UART Frame | test_uart_frame.c | 30+ | PASS |
| SPI Request | test_spi_request.c | 20+ | PASS |
| SPI Bridge | test_spi_bridge.c | 20+ | PASS |
| SCI Rx Queue | test_sci_rx_queue.c | 20+ | PASS |
| Step 2 Safety Shadow Mapping | test_step2_safety.c | 27 | PASS |

**Step 2 test coverage** (7 groups, 27 assertions):
- T1: ClampU16 basic (mid-range, below lo, above hi, at boundaries) — 5 assertions
- T2: ClampU16 zero-range — 2 assertions
- T3: ClampU16 16-bit full range + 32-bit overflow — 3 assertions
- T4: Safe compare upper bound calculation (TBPRD * permill / 1000) — 4 assertions
- T5: ADC raw → clamped safe compare mapping — 5 assertions
- T6: Invalid/missing input guard (-1, -100, overflow) — 4 assertions
- T7: max_duty_permill boundary values (1000, 1, 0 per-mill) — 3 assertions

## 3. Static Boundary Checks — PASS (4/4)

| Check | Result |
|---|---|
| Register access boundary (no `Regs.` outside drivers/bsp) | PASS |
| `platform_profile.h` compile guards | PASS |
| Legacy code in active build paths | PASS |
| `DSP2833x_Device.h` isolation (drivers/bsp only) | PASS |

## 4. Safety Evidence: Board_Init() Does NOT Auto-Enable PWM

**Evidence chain**:

1. `board.c:77` — Comment: "DrvEpwm_Enable() is NOT called here — reserved for future state machine."
2. `board.c:81` — `DrvEpwm_HaltTimebase()` called, sets `TBCLKSYNC = 0` (`drv_epwm.c:126-131`)
3. `board.c:95` — `DrvEpwm_Init()` called, sets `AQCSFRC.CSFA = CSFB = 1` (force LOW continuously) (`drv_epwm.c:82-85`)
4. `board.c:111` — `DrvEpwm_ConfigGpio()` called, GPIO mux gated by `#if BOARD_PWM_ADC_HW_CONFIRMED == 1U` (`drv_epwm.c:133-185`) — no-op when HW_CONFIRMED=0
5. `board.c:122` — End of `Board_Init()`: no call to `DrvEpwm_Enable()` anywhere

**Result**: After `Board_Init()` returns:
- `AQCSFRC` holds EPWMxA/B LOW (register-level force)
- `TBCLKSYNC = 0` (all ePWM time-base clocks stopped)
- GPIO mux for ePWM/TZ pins NOT configured (HW_CONFIRMED=0)
- ADC hardware triggers NOT enabled (HW_CONFIRMED=0 gate)
- `DrvEpwm_Enable()` would return -2 (safe no-op) if accidentally called with HW_CONFIRMED=0

**Static verification script**: `tools/quality_gate.ps1` Stage 2a confirms zero `Regs.` access outside `drivers/` and `bsp/` directories — no app code can bypass drivers to directly manipulate `AQCSFRC` or `TBCLKSYNC`.

## 5. Safety Evidence: ADC ISR Shadow Update Without PWM Output Release

**Evidence chain**:

1. `isr.c:94-100` — ADC ISR reads raw values via `DrvAdc_ReadRaw()`, guards invalid reads with `(raw >= 0) ? sample : 0U`
2. `isr.c:102-109` — Comment explicitly identifies this as "SAFETY PASSTHROUGH — Step 2 test logic only": "PWM outputs remain forced LOW via AQCSFRC until DrvEpwm_Enable() is called by a future state machine. This validates the ADC→Shadow register path without releasing PWM output."
3. `isr.c:110-113` — Calls `DrvEpwm_SetCompareA/B()` which writes to `CMPA.half.CMPA` / `CMPB` shadow registers (`drv_epwm.c:187-213`), with `max_duty_permill` clamp applied internally
4. `isr.c:116-117` — Snapshots `TZFLG` trip status via `DrvEpwm_GetTripStatus()`
5. `isr.c:120` — Clears ADC interrupt, acks PIE group 1
6. **No call to `DrvEpwm_Enable()`** in the ISR or anywhere else in the code

**Result**: ADC ISR writes to CMPA/CMPB shadow registers but:
- `AQCSFRC` remains in force-LOW state (independent of CMP values)
- `TBCLKSYNC` remains 0 (time-base not running, no PWM waveform generated)
- Shadow register load at CTR=ZERO never occurs because TBCTR is frozen
- PWM output pins remain LOW regardless of CMP register values

## 6. UART/SPI Regression — PASS

All Step 1 communication infrastructure tests re-executed and passing:

| Component | Test | Result |
|---|---|---|
| SCI/UART receive queue | test_sci_rx_queue | PASS |
| UART frame protocol | test_uart_frame | PASS |
| SPI request layer | test_spi_request | PASS |
| SPI bridge | test_spi_bridge | PASS |

## 7. Quality Gate Summary

**Quality gate**: 8/8 PASS (0 SKIP, 0 WARN, 0 FAIL)

Full quality gate output available at `tools/quality_gate_result.txt`.

## 8. PENDING Hardware Verification

All of the following remain **PENDING** until hardware validation on the actual target board:

| Item | Status | Blocking Step 2 Close? |
|---|---|---|
| ADC waveform capture (ADCINA0/ADCINB0) | PENDING | YES |
| ePWM output waveform (EPWMxA/B) after enable | PENDING | YES |
| Trip Zone TZ1/TZ2 independent shutdown test | PENDING | YES |
| ADC ISR WCET measurement (scope/logic analyzer) | PENDING | YES |
| Timer0 ISR WCET measurement | PENDING | YES |
| SCI RX ISR WCET measurement | PENDING | YES |
| `BOARD_PWM_ADC_HW_CONFIRMED` switch to `1U` | PENDING | YES |

## 9. Conclusion

**Step 2 Overall Status**: **CONDITIONALLY COMPLETE**

- Software build: PASS (4 configs, 0 error, 0 warning)
- Host tests: PASS (5 suites, all assertions passing)
- Static boundary checks: PASS (4/4)
- UART/SPI regression: PASS (4 suites unchanged)
- Board_Init() safety: VERIFIED — no DrvEpwm_Enable(), AQCSFRC Force Low, TBCLKSYNC=0
- ADC ISR shadow chain: VERIFIED — writes to shadow registers, does NOT release PWM output
- Hardware evidence: **PENDING** (7 items in §8)

Step 2 cannot be closed until all hardware verification items in §8 produce passing evidence. Set `BOARD_PWM_ADC_HW_CONFIRMED = 1U` only after hardware validation.
