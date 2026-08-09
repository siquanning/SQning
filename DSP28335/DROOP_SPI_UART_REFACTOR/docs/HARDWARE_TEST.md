# HARDWARE_TEST — DROOP_SPI_UART_REFACTOR

## 1. Test Infrastructure

### 1.1 Equipment

| Item | Model/Spec | Purpose |
|------|-----------|---------|
| DSP board | TMS320F28335 custom board | Device under test |
| JTAG probe | XDS100v3 | Debug, register inspection, Flash programming |
| USB-to-UART adapter | FTDI-based (3.3V TTL) | SCI-A data injection (GPIO35 TX, GPIO36 RX) |
| CPLD slave | On-board CPLD | SPI-A target (GPIO16 MOSI, GPIO17 MISO, GPIO18 CLK) |
| Oscilloscope | [ ] 未验证 | Physical waveform verification |
| Logic analyzer | [ ] 未验证 | Multi-channel timing measurement |

### 1.2 Software Tools

| Tool | Use |
|------|-----|
| CCS 20.5.1+ | Debug perspective: register read, memory browser, breakpoints, expressions |
| CCS serial MCP | Serial console (COM port read/write via FTDI D2XX) |
| PowerShell `build.ps1` | Flash Release build |
| `tests/host/_all.bat` | MSVC host test compilation + execution |

### 1.3 Connections

```
PC ──[USB]──► XDS100v3 ──[JTAG]──► F28335
PC ──[USB]──► FTDI UART ──[3.3V TTL]──► F28335 SCI-A (GPIO35/36)
F28335 SPI-A (GPIO16/17/18) ──► On-board CPLD
```

## 2. Test Categories

### 2.1 Test Matrix Overview

| # | Category | Host Tests | Hardware Tests | Critical |
|---|----------|-----------|----------------|----------|
| 1 | Build | — | RAM Debug + Flash Release | Yes |
| 2 | UART Frame | 9/9 PASS | 1/64/65 byte, gap boundary, consecutive | Yes |
| 3 | SPI Request | 14/14 PASS | Byte timing, timeout, MISO check | Yes |
| 4 | SPI Bridge | 10/10 PASS | End-to-end UART→SPI forwarding | Yes |
| 5 | Queue (SCI RX) | 6/6 PASS | Overflow prevention, stress | Yes |
| 6 | Scheduler | N/A | Miss counters, 1/10/100ms period | Yes |
| 7 | Diagnostics / WCET | N/A | Cycle counter, ISR WCET | No |
| 8 | Register Consistency | — | vs baseline snapshot | Yes |
| 9 | LED Indicator | N/A | GPIO67/68 toggling | No |
| 10 | Flash Boot | — | Standalone power-on boot | Yes |

## 3. Test Procedures

### 3.1 Build Verification

**Purpose**: Confirm both configurations compile without errors.

**Procedure**:
1. CCS IDE: build Debug configuration → expected: 0 errors, 1 warning
2. PowerShell: `.\Release\build.ps1` → expected: `SUCCESS: Release (Flash) build complete`
3. Verify map file shows `codestart` at 0x33FFF6 (Release only)
4. Verify map file shows `ramfuncs` with both LOAD (Flash) and RUN (RAM) addresses

**Expected**: 0 errors both builds.

**Status—Debug**: [x] PASS (2026-08-09)  
**Status—Release**: [x] PASS (2026-08-09)

---

### 3.2 UART Frame Parsing (Hardware)

**Purpose**: Verify UART frame detection on real SCI-A hardware.

**Procedure**:
1. Load RAM Debug build via JTAG, resume execution
2. Open serial console at 9600 bps 8N1
3. **Test 3.2.1 — 1-byte frame**: Send single byte `A` (0x41), wait 50ms
   - Read `g_app.spi_bridge.uart.rx_bytes` → +1
   - Read `g_app.spi_bridge.uart.ready_frames` → +1
4. **Test 3.2.2 — 64-byte frame**: Send 64×`B` (0x42) as contiguous burst, wait 50ms
   - Read `ready_frames` → +1
   - Read `g_app.spi_bridge.uart.rx_bytes` → +64
5. **Test 3.2.3 — 65-byte overlong**: Send 65×`C` (0x43) as contiguous burst, wait 50ms
   - Read `g_app.spi_bridge.uart.too_long_frames` → +1
   - Read `ready_frames` → unchanged
6. **Test 3.2.4 — Gap boundary (3.9ms vs 4.0ms)**: [ ] 未验证
   - Send bytes with 3.9ms gap → should NOT trigger frame boundary
   - Send bytes with 4.0ms gap → SHOULD trigger frame boundary
   - Requires precise inter-byte timing control not available via standard serial terminal
7. **Test 3.2.5 — Consecutive frames**: Send two 1-byte frames with >4ms gap between them
   - Read `ready_frames` → +2
   - Read `g_app.spi_bridge.frames_forwarded` → +2

**Expected**: Counter deltas match expected values exactly.

**Status—3.2.1**: [x] PASS (2026-08-09)  
**Status—3.2.2**: [x] PASS (2026-08-09)  
**Status—3.2.3**: [x] PASS (2026-08-09)  
**Status—3.2.4**: [ ] 未验证 — requires precision inter-byte timing  
**Status—3.2.5**: [x] PASS (2026-08-09, Test 5: 20×64-byte consecutive frames)

---

### 3.3 SPI Byte Transfer (Hardware)

**Purpose**: Verify SPI-A byte-level state machine with real CPLD slave.

**Procedure**:
1. Load RAM Debug build via JTAG, resume execution
2. Send a 1-byte UART frame → triggers SPI transfer
3. Read `g_app.spi_bridge.spi.req_bytes` → should increment per byte sent
4. Read `g_app.spi_bridge.spi.miso_idle_ff` → counts MISO=0xFF responses
5. Read `g_app.spi_bridge.spi.miso_unexpected` → counts MISO≠0xFF responses (CPLD responding)
6. Read `g_app.spi_bridge.spi.timeouts` → should be 0 (CPLD always responds)

**Expected**: `req_bytes` matches UART frame size; `miso_unexpected > 0` (CPLD connected and responding); `timeouts = 0`.

**Status**: [x] PASS (2026-08-09, verified with 1/64/65 byte frames)

### 3.3.1 SPI Timeout (Hardware Fault Injection)

**Purpose**: Verify 5ms SPI byte timeout when slave is disconnected.

**Procedure**: [ ] 未验证
1. Physically disconnect CPLD (or force MISO high-impedance)
2. Send a 1-byte UART frame → SPI transfer starts
3. Read `g_app.spi_bridge.spi.timeouts` → should increment after 5ms
4. Read `g_app.spi_bridge.frames_forwarded` → should NOT increment (frame dropped)

**Status**: [ ] 未验证 — Host test validates timeout state machine; no physical SPI bus fault was created.

---

### 3.4 End-to-End UART→SPI Bridge (Hardware)

**Purpose**: Verify complete pipeline from UART byte to SPI byte.

**Procedure**:
1. Load RAM Debug build, resume
2. Zero all counters (JTAG disconnect + reconnect)
3. Send 1-byte frame `A` (0x41):
   - Read: `rx_bytes=+1, ready_frames=+1, req_frames=+1, req_bytes=+1, frames_forwarded=+1`
4. Send 64-byte frame `B`×64:
   - Read: `rx_bytes=+64, ready_frames=+1, req_frames=+1, req_bytes=+64`
5. Send 20×64-byte frames with proper inter-frame gaps (>70ms):
   - Read: `rx_bytes=+1280, ready_frames=+20, req_frames=+20, req_bytes=+1280`
   - Read: `busy_drops=0, overflow=0, timeouts=0`

**Expected**: Conservation law `rx_bytes = req_bytes + busy_drops`; `ready_frames = req_frames = frames_forwarded`; zero drops with proper gaps.

**Status**: [x] PASS (2026-08-09, Test 5: 20×64-byte zero-drop stress test)

---

### 3.5 Queue Overflow Prevention (Hardware)

**Purpose**: Verify SPSC queue doesn't overflow under load.

**Procedure**:
1. Load RAM Debug build, resume
2. Send rapid bursts of data without inter-frame gaps
3. Read `g_app.sci_rx_queue.overflow_count` → should be 0 (128 effective capacity is ample)
4. Read `g_app.spi_bridge.uart.busy_drops` → may be >0 (frame-level busy-drop, not queue overflow)

**Expected**: `overflow_count = 0` for all practical data rates at 9600 bps.

**Status**: [x] PASS (2026-08-09, Test 4: 24 × 64-byte burst, overflow=0)

---

### 3.6 Scheduler (Hardware)

**Purpose**: Verify multi-rate scheduler fires correctly on DSP hardware.

**Procedure**:
1. Load RAM Debug build, resume, let run for several seconds
2. Pause via JTAG
3. Read scheduler state: `g_sched` at 0xC000
   - `last_1ms`, `last_10ms`, `last_100ms` → reasonable values (not 0)
   - `miss_1ms`, `miss_10ms`, `miss_100ms` → 0 (no misses)
4. Resume, let run longer, pause again
   - Miss counters should remain 0
5. Read diagnostics snapshot (at 100ms): `g_diagnostics.miss_1ms` etc. → confirms snapshots working

**Expected**: All miss counters = 0 under normal operation; task slots fire at correct periods.

**Status**: [x] PASS (2026-08-09, all miss counters = 0 after Step 3 Scheduler implementation)

---

### 3.7 Diagnostics / WCET (Hardware)

**Purpose**: Verify CPU Timer2 cycle counter and WCET measurements.

**Procedure**:
1. Load RAM Debug build, resume, let run for several seconds
2. Pause via JTAG
3. Read Diagnostics struct at 0xC2C0:
   - `timer0_isr.min_cycles` → should be < `max_cycles`
   - `sci_rx_isr.min_cycles` → should be < `max_cycles`
   - `main_loop.min_cycles` → should be < `max_cycles`
   - All `last_cycles` fields → non-zero (one measurement since last pause)
4. Verify Timer2 is counting: read `CpuTimer2Regs.TIM.all` twice — values should differ (free-running down-counter)

**Expected**: All WCET slots have valid min/max/last values; Timer2 is counting.

**Status**: [ ] 未验证 — Diagnostics module compiled and linked (confirmed in map file, 60 bytes code + 28 bytes data); WCET values not yet read from running hardware.

---

### 3.8 Register Consistency (Hardware)

**Purpose**: Verify all peripheral registers match baseline after refactoring.

**Procedure**:
1. Load RAM Debug build, resume briefly, pause
2. Read registers via CCS Expressions view:

| Register | Address | Expected Value | Meaning |
|----------|---------|---------------|---------|
| PLLSTS | 0x7011 | 0x0101 | PLL locked, DIVSEL=1 (/2) |
| HISPCP | 0x701A | 0x0001 | HSPCLK = 75 MHz |
| LOSPCP | 0x701B | 0x0002 | LSPCLK = 37.5 MHz |
| SCICCR | 0x7050 | 0x0007 | 8-bit, idle-line, 1 stop, no parity |
| SCIHBAUD | 0x7052 | 0x0001 | BRR high byte |
| SCILBAUD | 0x7053 | 0x00E7 | BRR = 487 → 9600 bps |
| SPICCR | 0x7040 | 0x0087 | SWRESET=1, Mode 0, 8-bit |
| SPIBRR | 0x7044 | 0x007F | ~293 kHz |
| SPIFFTX | 0x704A | 0x8000 | SPIRST=1, FIFO disabled |
| CpuTimer0 TCR | 0x0C04 | 0x4001 | TIE=1, TSS=0 (running) |

**Expected**: All registers match baseline values exactly.

**Status—Step 1**: [x] PASS (2026-08-09, 9/9 registers match)  
**Status—Step 2**: [x] PASS (2026-08-09, 10/10 registers match)  
**Status—Step 3**: [x] PASS (2026-08-09, 6/6 registers match)  
**Status—Step 4**: [ ] 未验证 — registers should be re-read after Diagnostics module integration to confirm no side effects

---

### 3.9 LED Indicator (Hardware)

**Purpose**: Verify GPIO67/68 LED toggling.

**Precondition**: `Indicator_TriggerRx()` and `Indicator_TriggerTx()` currently have **no callers** — this is a known architecture issue (§2.1 of implementation plan). LEDs will not illuminate.

**Status**: [ ] 未验证 — API exists (`Indicator_Init`, `Indicator_TriggerRx/Tx`, `Indicator_Service`); no callers wired. LEDs stay off. This is expected current behavior, not a regression.

---

### 3.10 Flash Standalone Boot (Hardware)

**Purpose**: Verify firmware boots from Flash without JTAG.

**Procedure**: [ ] 未验证
1. Build Flash Release via `.\Release\build.ps1`
2. Program Flash via CCS (Run → Debug with Release configuration)
3. Verify program loaded successfully (check codestart at 0x33FFF6)
4. **Run → Terminate** (disconnect JTAG)
5. Power-cycle the board (remove and restore power)
6. Open serial console at 9600 bps
7. Send 1-byte frame `A` (0x41)
8. Reconnect JTAG, pause, read diagnostic counters
9. Verify `rx_bytes ≥ 1` and `frames_forwarded ≥ 1`

**Expected**: Board boots autonomously on power-up; UART→SPI bridge functions without JTAG attached.

**Status**: [ ] 未验证 — Flash Release compiled successfully (0 errors); Flash programming and standalone boot test not yet executed on hardware.

---

### 3.11 Flash Release Register Verification

**Purpose**: Verify peripheral registers are identical between Debug and Release builds after boot.

**Procedure**: [ ] 未验证
1. Program Flash Release, boot standalone
2. Attach JTAG (hot-attach, not reset)
3. Read registers per §3.8 table
4. Compare against Debug register values

**Expected**: All peripheral registers match Debug build values. Flash execution (with 5 wait states) may differ in timing but must not differ in register configuration.

**Status**: [ ] 未验证

---

## 4. Unverified Items Summary

| # | Item | Reason | Priority |
|---|------|--------|----------|
| 1 | Flash standalone boot + power-cycle test | Not yet executed on hardware | High |
| 2 | Flash Release register verification | Depends on #1 | High |
| 3 | Diagnostics WCET values read from running HW | Not yet read via JTAG | Medium |
| 4 | Step 4 register consistency re-check | Diagnostics adds Timer2 config; verify no side effects | Medium |
| 5 | UART gap boundary 3.9ms vs 4.0ms | Requires precision inter-byte timing control | Low |
| 6 | SPI timeout via hardware fault injection | Requires physical bus disconnection | Low |
| 7 | SPI MISO 0xFF counting (idle bus) | CPLD normally connected and responding | Low |
| 8 | LED GPIO67/68 toggling | `Indicator_TriggerRx/Tx` have no callers (known issue) | Low |
| 9 | Oscilloscope: SCI-A bit timing at 9600 bps | Register values confirmed; no physical measurement | Low |
| 10 | Oscilloscope: SPI-A SCK ~293 kHz + Mode 0 phase | Register values confirmed; no physical measurement | Low |

## 5. Test Execution Log

| Date | Step | Tests Executed | Result |
|------|------|---------------|--------|
| 2026-08-09 | Step 1 | Build, UART 1/64/65, Register, JTAG | PASS (8/8 UART + 13/13 SPI host) |
| 2026-08-09 | Step 2 | Build, UART 1/64/65, Register, JTAG, Static boundaries | PASS (21/21 host, 10/10 register match) |
| 2026-08-09 | Step 3 | Build, Queue, Bridge, UART 1/64/65, Stress, Scheduler | PASS (39/39 host, zero drops, zero overflows) |
| 2026-08-09 | Step 4C | Flash Release build, map verification | PASS (0 errors, codestart @0x33FFF6) |
| 2026-08-09 | Step 4D | Diagnostics compile + link | PASS (60 bytes code + 28 bytes data in map) |
| — | Step 4E | Flash boot, WCET read, Step 4 register check | 未验证 |

## 6. Quick Regression Checklist

After any code change, run this minimum set:

- [ ] CCS Debug build (0 errors)
- [ ] `.\Release\build.ps1` (0 errors)
- [ ] `tests\host\_all.bat` (39/39 PASS)
- [ ] JTAG download + resume
- [ ] 1-byte UART→SPI test (counter audit)
- [ ] 64-byte UART→SPI test (counter audit)
- [ ] Register snapshot vs baseline (PLLSTS, SCICCR, SCIHBAUD, SCILBAUD, SPICCR, SPIBRR, SPIFFTX, CpuTimer0 TCR)

This takes approximately 5 minutes on a connected setup.
