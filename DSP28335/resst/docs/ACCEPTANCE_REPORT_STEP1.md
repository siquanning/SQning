# PRD Step 1 — Final Acceptance Report

**Date:** 2026-08-10
**Project:** F28335_RTControl_Platform
**Canonical Directory:** `E:\repos\DSP28335\F28335_RTControl_Platform`
**Git Branch:** main
**Report Status:** FINAL

---

## Summary

| # | Criterion | Verdict | Evidence |
|---|-----------|---------|----------|
| 1 | Unified Identity | **PASS** | Zero old-name refs in scripts/CCS metadata; docs labeled historical |
| 2 | Directory Migration | **PASS** | Single canonical directory; old dir + .bak cleaned; git rename detected |
| 3 | Four Build Configurations | **PASS** | 4 configs in .cproject with unique defines + linkers per config |
| 4 | Build Verification | **PASS** | RAM Debug + Flash Release built via quality gate, 0e/0w |
| 5 | Register Access Boundary | **PASS** | Timer2 encapsulated in drivers/; static boundary checks pass |
| 6 | Acceptance Report | **PASS** | This document |

**Quality Gate:** 8/8 PASS, 0 SKIP, 0 WARN, 0 FAIL (run 2026-08-10 from canonical directory)

---

## 1. Unified Identity — PASS

**Requirement:** Replace all DROOP_SPI_UART_REFACTOR references in current docs/scripts/artifacts with F28335_RTControl_Platform. Historical baselines may retain old names but must be explicitly labeled.

### Evidence

**Updated documents (active identity):**
- [docs/BUILD_AND_FLASH.md](BUILD_AND_FLASH.md) — Title, build identity table, output names
- [docs/ARCHITECTURE.md](ARCHITECTURE.md) — Title + description with "formerly known as" annotation
- [docs/MEMORY_LAYOUT.md](MEMORY_LAYOUT.md) — Title updated
- [docs/TASK_TIMING.md](TASK_TIMING.md) — Title updated
- [docs/HARDWARE_TEST.md](HARDWARE_TEST.md) — Title updated
- [docs/ALGORITHM_CONTRACT.md](ALGORITHM_CONTRACT.md) — Title + inline "formerly DROOP_SPI_UART_REFACTOR"
- [README.md](../README.md) — Quick start sections reference new CCS config names

**Historical records (labeled):**
- [docs/四步重构实施方案.md](四步重构实施方案.md) — Title suffix "（历史来源: DROOP_SPI_UART_REFACTOR）", banner on line 5
- [tests/hardware/BASELINE_TEST_RECORD.md](../tests/hardware/BASELINE_TEST_RECORD.md) — Title suffix "（历史基线）", warning banner on line 3
- `tests/hardware/baseline_artifacts/` — Old-name .out/.map preserved as historical snapshots

**Clean artifacts (zero old-name references):**
- `.ccsproject` — No DROOP_SPI_UART_REFACTOR found
- `.cproject` — No DROOP_SPI_UART_REFACTOR found
- `.project` — No DROOP_SPI_UART_REFACTOR found
- All `*.ps1`, `*.bat`, `*.cmd` scripts — No DROOP_SPI_UART_REFACTOR found
- Quality gate outputs — `F28335_RTControl_Platform.out`, `F28335_RTControl_Platform.map`

### Residue Inventory

| File | Old Name Reference | Verdict |
|------|-------------------|---------|
| docs/ARCHITECTURE.md:5 | "formerly known as DROOP_SPI_UART_REFACTOR" | Allowed — explicit historical annotation |
| docs/ALGORITHM_CONTRACT.md:12 | "(formerly DROOP_SPI_UART_REFACTOR)" | Allowed — explicit historical annotation |
| docs/BUILD_AND_FLASH.md:228 | Historical note about old build names | Allowed — section 9 "Historical Note" |
| docs/PRD_F28335_RTCONTROL_PLATFORM.md:9 | "当前来源" field | Allowed — provenance record |
| docs/四步重构实施方案.md:1,5,213,834 | Implementation history refs | Allowed — title + banner both label as historical |
| tests/hardware/BASELINE_TEST_RECORD.md | Historical test baselines | Allowed — title + warning banner label as historical |
| tests/hardware/baseline_artifacts/*.out/.map | Old-name build artifacts | Allowed — frozen historical snapshots |
| README.md:111 | "前身为 DROOP_SPI_UART_REFACTOR" | Allowed — provenance note |

---

## 2. Directory Migration — PASS

**Requirement:** Don't keep two independently buildable project copies. After migration, confirm CCS project, scripts, README, and commands all point to the single canonical directory.

### Evidence

- **Single canonical directory:** `E:\repos\DSP28335\F28335_RTControl_Platform`
- **Old directory:** `DROOP_SPI_UART_REFACTOR` — deleted (via robocopy /MOVE)
- **Backup copy:** `_F28335_RTControl_Platform.bak` — deleted
- **Git rename detection:** All files show `R` (rename) status in git index
- **No other copies:** `ls E:/repos/DSP28335/ | grep -i f28335` returns exactly one entry

### Migration Method

```
robocopy "DROOP_SPI_UART_REFACTOR" "F28335_RTControl_Platform" /E /MOVE
git add F28335_RTControl_Platform/
git add -u DROOP_SPI_UART_REFACTOR/
```

Note: Direct `mv`/`rename` was blocked by a Windows file lock (diagnosed as non-CCS handle, likely Search Indexer or Defender). `robocopy /MOVE` succeeded where `mv` and `Rename-Item` failed.

---

## 3. Four Build Configurations — PASS

**Requirement:** Establish and solidify in CCS project metadata: Prototype_RAM_Debug, Prototype_Flash_Demo, Industrial_RAM_Debug, Industrial_Flash_Release. Each must inject unique PLATFORM_PROFILE_* defines with RAM/Flash linker differences. Not accepting "user manually adds -D parameter" as delivery.

### Evidence

All four configurations are embedded in [`.cproject`](../.cproject) (`configRelations="4"`):

| CCS Configuration | Build Identity | Defines | Linker CMD | Output |
|-------------------|---------------|---------|------------|--------|
| Debug | Prototype_RAM_Debug | `PLATFORM_PROFILE_PROTOTYPE` | `linker/28335_RAM_lnk.cmd` | F28335_RTControl_Platform.out |
| Release | Prototype_Flash_Demo | `PLATFORM_PROFILE_PROTOTYPE`, `FLASH` | `linker/f28335_flash.cmd` | F28335_RTControl_Platform.out |
| Industrial_RAM | Industrial_RAM_Debug | `PLATFORM_PROFILE_INDUSTRIAL` | `linker/28335_RAM_lnk.cmd` | F28335_RTControl_Platform.out |
| Flash_Release | Industrial_Flash_Release | `PLATFORM_PROFILE_INDUSTRIAL`, `FLASH` | `linker/f28335_flash.cmd` | F28335_RTControl_Platform.out |

**Supporting infrastructure:**
- [firmware/platform_profile.h](../firmware/platform_profile.h) — `#error` guards (neither/both defined), capability macros per profile, `PLATFORM_BUILD_ID` resolution for all 4 variants
- Quality gate `Invoke-C2000Build` helper — programmatic compilation with arbitrary defines + linker
- CCS IDE — switch active build configuration via Project → Build Configurations → Set Active

The defines are injected at the compiler level within each `.cproject` configuration block. No manual `-D` parameter is required from the user.

---

## 4. Build Verification — PASS

**Requirement:** Build at minimum RAM Debug and Flash Release, report 0e/0w. Flash build must generate new platform-named .out/.map. Old-name artifacts must not appear as current release artifacts. Quality gate must actually invoke and check these builds.

### Evidence — Quality Gate Run (2026-08-10)

```
=== Stage 1: Host Tests ===
  [PASS] Host Tests (4 suites)

=== Stage 2: Static Boundary Checks ===
  [PASS] Register access boundary (app/services/control/algorithm)
  [PASS] platform_profile.h compile guards
  [PASS] Legacy code in active paths
  [PASS] DSP2833x_Device.h isolation (drivers/bsp only)

=== Stage 3: RAM Debug Build (Prototype_RAM_Debug) ===
  [PASS] Build: Prototype_RAM_Debug
         Output: F28335_RTControl_Platform/Debug/gate_build/F28335_RTControl_Platform.out
         Map:    F28335_RTControl_Platform/Debug/gate_build/F28335_RTControl_Platform.map

=== Stage 4: Flash Release Build (Industrial_Flash_Release) ===
  [PASS] Build: Industrial_Flash_Release
         Output: F28335_RTControl_Platform/Flash_Release/gate_build/F28335_RTControl_Platform.out
         Map:    F28335_RTControl_Platform/Flash_Release/gate_build/F28335_RTControl_Platform.map

=== Quality Gate Summary ===
  PASS: 8  SKIP: 0  WARN: 0  FAIL: 0
  QUALITY GATE: PASSED
```

**Build artifacts (verified):**
- `Debug/gate_build/F28335_RTControl_Platform.out` — 176,408 bytes (Prototype_RAM_Debug)
- `Debug/gate_build/F28335_RTControl_Platform.map` — 41,031 bytes
- `Flash_Release/gate_build/F28335_RTControl_Platform.map` — 42,535 bytes (Industrial_Flash_Release)
- `Flash_Release/gate_build/F28335_RTControl_Platform.out` — 177,492 bytes

**Quality gate mechanism:** The `Invoke-C2000Build` PowerShell function in [tools/quality_gate.ps1](../tools/quality_gate.ps1) invokes `cl2000.exe` directly (not a pre-check wrapper). Compilation uses `--preproc_with_compile` to generate dependency files. Linker errors and warnings are captured from stderr. The gate does not SKIP these stages — it actually compiles and links all 25 source files for each configuration.

---

## 5. Register Access Boundary — PASS

**Requirement:** Handle firmware/app/diagnostics.c's direct access to CpuTimer2Regs. Encapsulate Timer2 init/read into Drivers. Update Host tests/static boundary checks to ensure app, services, control, algorithm don't directly access TI registers.

### Evidence

**Code changes:**
- [firmware/drivers/drv_timer.h](../firmware/drivers/drv_timer.h) — Added `DrvTimer2_CycleInit()` and `DrvTimer2_CycleRead()` declarations
- [firmware/drivers/drv_timer.c](../firmware/drivers/drv_timer.c) — Added Timer2 implementation (lines 49-60)
- [firmware/app/diagnostics.c](../firmware/app/diagnostics.c) — Removed `#include "DSP2833x_Device.h"`; now calls `DrvTimer2_CycleInit()` and `DrvTimer2_CycleRead()` via driver API

**Static boundary check results (quality gate Stage 2):**
- Register access boundary: **PASS** — "No direct Regs. access outside drivers/ and bsp/"
- DSP2833x_Device.h isolation: **PASS** — Zero violations outside drivers/ and bsp/
- Check covers: app/, services/ (no control/ or algorithm/ directories currently exist; checks are in place when added)

**Scope of checks:**
- Regs. access pattern: `Regs\.` in non-driver/non-bsp/non-reference source files
- DSP2833x_Device.h includes: `#include "DSP2833x_Device.h"` outside allowed directories

---

## 6. PENDING Items

The following items cannot be verified without hardware (XDS100v3 debug probe + TMS320F28335 target board). They are explicitly marked PENDING, not DONE or VERIFIED.

### JTAG 1/64/65 Byte Regression — PENDING

**Description:** Verify SPI UART bridge handles frame lengths of 1, 64, and 65 bytes correctly over JTAG-debugged hardware. These boundary values exercise the SCI RX queue and UART frame parser at their limits.

**Manual execution steps:**
1. Connect XDS100v3 to target board; power on
2. Launch CCS, import `F28335_RTControl_Platform` project
3. Build Debug configuration (Prototype_RAM_Debug)
4. Debug → load program to target
5. Connect serial terminal to SCI-A (115200 8N1)
6. Send test frames: 1 byte, 64 bytes, 65 bytes (with valid SOF/EOF markers)
7. Observe SPI output (MOSI + SCLK) on logic analyzer or scope
8. Verify: each SPI byte matches the UART payload byte; frame boundaries respected

**Historical baseline:** [tests/hardware/BASELINE_TEST_RECORD.md](../tests/hardware/BASELINE_TEST_RECORD.md) §JTAG records — these were recorded under the old project name and should be re-verified with the current `F28335_RTControl_Platform.out` binary.

### Flash Hardware Programming — PENDING

**Description:** Verify Industrial_Flash_Release binary programs successfully to on-chip Flash via JTAG, boots standalone after power cycle, and runs with correct PLATFORM_PROFILE_INDUSTRIAL behavior.

**Manual execution steps:**
1. Connect XDS100v3 to target board
2. Build Flash_Release configuration in CCS (Industrial_Flash_Release)
3. Debug → load program to Flash (CCS will invoke Flash programmer)
4. Verify programming completes without error
5. Disconnect JTAG, power-cycle board
6. Verify standalone boot: LED/heartbeat pattern, UART bridge responds
7. Confirm build identity via diagnostic output: `PLATFORM_BUILD_ID` = "Industrial_Flash_Release"

### Prototype_Flash_Demo + Industrial_RAM_Debug Build Verification — PENDING (low priority)

The quality gate currently builds the two "corner" configurations (Prototype_RAM_Debug and Industrial_Flash_Release). The other two (Prototype_Flash_Demo and Industrial_RAM_Debug) exist in `.cproject` with correct defines and linkers but are not exercised by the automated gate. Extending the gate to build all 4 is a straightforward enhancement.

---

## Quality Gate Result Archive

Full output saved to: `tools/quality_gate_result.txt` (2026-08-10 run)

Command: `powershell -ExecutionPolicy Bypass -File tools/quality_gate.ps1`
Working directory: `E:\repos\DSP28335\F28335_RTControl_Platform`
Compiler: `ti-cgt-c2000_25.11.0.LTS\bin\cl2000.exe`
Host compiler: MSVC 2022 Community (vswhere-detected)

---

## Verdict

| Gate | Verdict | Detail |
|------|---------|--------|
| 第一步·工程软件验收 | **PASS** | 6 项准则全部通过；质量门 8/8 PASS, 0 SKIP |
| 第一步·硬件回归验收 | **PENDING** | 需 XDS100v3 + TMS320F28335 目标板 |
| **PRD 第一步 总体** | **CONDITIONALLY COMPLETE** | 待硬件证据关闭 |

All 6 software-verifiable criteria: **PASS**. Two hardware-dependent items (JTAG 1/64/65 byte regression + Flash on-chip programming): **PENDING** with executable manual steps documented in §6. No item is falsely marked DONE or VERIFIED.

**第二步准入判断：** 工程软件验收已通过，可开始第二步软件设计与实现准备。第二步期间必须保持 PWM 未接入、功率输出默认安全。硬件回归验证（JTAG 边界帧 + Flash 烧录）应在首次上电前完成。
