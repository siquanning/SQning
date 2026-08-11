# TASK_TIMING — F28335_RTControl_Platform

## 1. Time Base

| Parameter | Value | Source |
|-----------|-------|--------|
| SYSCLKOUT | 150 MHz | `BOARD_SYSCLK_MHZ` |
| CPU Timer0 period | 100 μs | `BOARD_TIMER0_PERIOD_US` |
| System tick counter | `uint32_t` wrapping at ~49.7 days | `Timebase_Now()` → `g_sysTick` |
| Timer0 ISR frequency | 10 kHz | Every 100 μs |
| CPU Timer2 (WCET) | 150 MHz free-running down-counter | `Diagnostics_CycleRead()` |
| WCET resolution | ~6.67 ns/tick | 1 / 150 MHz |
| WCET wraparound | ~28.6 s | 2^32 / 150 MHz |

## 2. Main Loop — Per Iteration

The main loop runs unconditionally. Every iteration executes these steps in order:

| Step | Function | Blocking? | Typical Cost |
|------|----------|-----------|-------------|
| 1 | `Timebase_Now()` | No | ~10 cycles (double-read on 16-bit bus) |
| 2 | WCET measurement (prev - now) | No | ~5 cycles |
| 3 | Drain `SciRxQueue` → `SpiBridge_OnRxByte/OnRxError` | No | 0 to N items; ~50 cycles/item |
| 4 | `SpiBridge_Service()` → `SpiRequest_Service()` → `DrvSpi_StartByte` / `DrvSpi_TryCompleteByte` | No | ~50-100 cycles (register read/write only) |
| 5 | `Indicator_Service()` | No | ~20 cycles (50ms timer check + GPIO set/clear) |
| 6 | `Scheduler_Take1ms()` | No | ~15 cycles (one unsigned subtraction + branch) |
| 7 | `Scheduler_Take10ms()` | No | ~15 cycles |
| 8 | `Scheduler_Take100ms()` | No | ~15 cycles + diagnostics snapshot (~50 cycles when firing) |

All steps are non-blocking. The loop has no `delay_ms()`, no `while`-spin on hardware flags, and no variable-length computation that could grow unboundedly. The dominant variable cost is step 3 (queue drain), which scales with the number of SCI bytes received since the last iteration.

## 3. Background Tasks (Scheduler)

### 3.1 Slot Allocation

| Slot | Period (ticks) | Period (time) | Miss Threshold | Current Use |
|------|---------------|---------------|----------------|-------------|
| 1ms | 10 | 1 ms | `elapsed ≥ 20 ticks` | Reserved for fast control |
| 10ms | 100 | 10 ms | `elapsed ≥ 200 ticks` | Reserved for droop outer loop / protection |
| 100ms | 1000 | 100 ms | `elapsed ≥ 2000 ticks` | Diagnostics snapshot |

### 3.2 Scheduler Merge Strategy

The scheduler uses a **phase-accumulator** model:

```
elapsed = now - last
if elapsed < period → return 0 (not yet)
if elapsed ≥ 2×period → miss++, last = now (resync, skip catch-up)
else → last += period, return 1 (normal fire)
```

Key properties:
- **No catch-up bursts**: When a slot is missed (elapsed ≥ 2×period), the scheduler resyncs `last` to `now` instead of calling the task repeatedly to catch up. This prevents a temporary overload from cascading into sustained CPU starvation.
- **Drift-free**: In the normal case (`period ≤ elapsed < 2×period`), `last` is advanced by exactly `period`, not reset to `now`. This prevents long-term phase drift — the task slot stays locked to the tick counter modulo `period`.
- **Wraparound-safe**: All elapsed computations use `uint32_t` unsigned subtraction (`now - last`), which is correct across the ~49.7-day counter wraparound.
- **Miss counter**: Saturating diagnostic — each slot tracks total missed firings since boot. Snapshot at 100ms.

### 3.3 100ms Diagnostics Snapshot

Once per 100ms, the scheduler task snapshots into the `Diagnostics` struct:

| Field | Source | Purpose |
|-------|--------|---------|
| `miss_1ms` | Scheduler.miss_1ms | Detect 1ms slot overload |
| `miss_10ms` | Scheduler.miss_10ms | Detect 10ms slot overload |
| `miss_100ms` | Scheduler.miss_100ms | Detect 100ms slot overload |
| `sci_rx_overflow` | SciRxQueue.overflow_count | Detect ISR→main queue full |

These are cumulative counters, not rates. A growing value indicates a systemic timing problem.

## 4. ISR Timing

### 4.1 Timer0 ISR (every 100 μs)

| Phase | Operation |
|-------|-----------|
| Entry | WCET start (read `CpuTimer2Regs.TIM.all`) |
| Tick advance | `g_sysTick++` (ISR-only writer, main reads via `Timebase_Now`) |
| Flag clear | `CpuTimer0Regs.TCR.bit.TIF = 1` |
| PIE ack | `PieCtrlRegs.PIEACK.all = PIEACK_GROUP1` |
| Exit | WCET measurement → `WcetUpdate(&timer0_isr, t0 - now)` |

**Deadline**: Must complete before the next Timer0 interrupt (100 μs).  
**Measured WCET**: [ ] 未验证 — WCET instrumentation integrated; values pending JTAG readout.

### 4.2 SCI-A RX ISR (on ≥1 byte in FIFO)

| Phase | Operation |
|-------|-----------|
| Entry | WCET start + `Timebase_Now()` for item timestamp |
| Guard | NULL-check on queue pointer |
| Error path | `DrvSci_HasError()` → push error item to queue |
| Data path | Read FIFO count → for each byte: check status, read byte → push to queue |
| Recovery | If any error → `DrvSci_RecoverRx()` |
| Ack | `DrvSci_ClearRxInterrupt()` + `PieCtrlRegs.PIEACK.all = PIEACK_GROUP9` |
| Exit | WCET measurement |

**Worst case**: FIFO full (16 bytes at current configuration, RXFFIL=1). Each byte requires status check + read + queue push. Recovery path adds SCI SW RESET overhead.  
**Deadline**: Must complete before the next byte at 9600 bps arrives (~1.04 ms). In practice, SCI-A FIFO depth provides buffering.  
**Measured WCET**: [ ] 未验证 — WCET instrumentation integrated; values pending JTAG readout.

## 5. Communication Timing

### 5.1 UART (SCI-A, 9600 bps 8N1)

| Parameter | Value | Ticks |
|-----------|-------|-------|
| Bit time | ~104.2 μs | ~1.04 ticks |
| Byte time (start + 8 data + stop) | ~1.04 ms | ~10.4 ticks |
| Max frame size | 64 bytes | — |
| Max frame duration (64 bytes) | ~66.7 ms | ~667 ticks |
| Frame gap (idle timeout) | 4 ms | 40 ticks |
| Overlong threshold | 65th byte | — |

### 5.2 SPI (SPI-A Master, ~293 kHz, Mode 0, 8-bit)

| Parameter | Value | Ticks |
|-----------|-------|-------|
| SCK frequency | ~293 kHz (LSPCLK=37.5MHz / 128) | — |
| Byte transfer time | ~34.1 μs (8 bits / 293 kHz) | ~0.34 ticks |
| Inter-byte gap | 1 ms | 10 ticks |
| Single-byte timeout | 5 ms | 50 ticks |
| Max 64-byte request time | ~66 ms (64 × (34 μs + 1 ms)) | ~660 ticks |

### 5.3 End-to-End Latency (UART reception → SPI completion)

For a 64-byte frame:

| Phase | Duration | Cumulative |
|-------|----------|------------|
| UART byte receipt (64 × 1.04ms) | ~66.7 ms | 66.7 ms |
| Frame gap detection | 4 ms | 70.7 ms |
| ISR → queue → main loop pick-up | <1 main loop iteration (~10-100 μs) | ~70.8 ms |
| SpiBridge → SpiRequest start | immediate (next Service call) | ~70.8 ms |
| SPI byte transfer (64 × ~34 μs + 63 × 1ms gap) | ~65 ms | ~135.8 ms |
| **Total** | **~136 ms** | |

The 1ms SPI inter-byte gap dominates. This is a deliberate pacing choice to avoid monopolizing the SPI bus from a slow CPLD slave.

## 6. Flash Boot Timing

| Phase | Duration | Notes |
|-------|----------|-------|
| Power-on → Boot ROM | <1 ms | Internal POR + crystal start-up |
| Boot ROM → codestart | negligible | Single branch |
| codestart → _c_int00 | negligible | `LB _c_int00` |
| RTS boot (cinit copy) | <1 ms | Zero-init .bss + copy .cinit → .data (both small) |
| `MemCopy()` ramfuncs | <100 μs | ~0x1F words copied from Flash to RAML03 |
| PLL lock | ~1 ms | XCLKOUT cycles waiting for PLLSTS[MCLKSTS] clear |
| `DrvFlash_Init()` | ~100 μs | Flash bank/pump power-up + wait-state register writes |
| Peripheral init | <1 ms | Timer0, SCI-A, SPI-A register writes |
| **Total to main loop** | **<5 ms** | Conservative estimate |

The critical constraint in Flash boot is that `MemCopy(ramfuncs)` must execute BEFORE `DrvSysCtrl_Init()` (which changes PLL), and `DrvFlash_Init()` must execute FROM RAM after the PLL is stable. The current `Board_Init()` ordering satisfies both.

## 7. LED Indicator Timing

| Parameter | Value | Notes |
|-----------|-------|-------|
| Pulse duration | 500 ticks = 50 ms | `LED_DURATION_TICKS` |
| Active state | Low (GPxCLEAR = on) | GPIO67/68 active-low |
| Callers | None | `Indicator_TriggerRx/Tx` APIs exist but are not yet wired |

## 8. Timing Constants (Source of Truth)

All timing constants derive from three files:

| File | Constants |
|------|-----------|
| `firmware/bsp/board_config.h` | SYSCLK, LSPCLK, baud, SPIBRR, timer period |
| `config/comm_config.h` | Tick intervals, frame capacity, microsecond→tick conversion, compile-time assertions |
| `firmware/app/scheduler.c` | 1ms/10ms/100ms tick counts (local `#define`) |

`comm_config.h` has compile-time assertions that fail the build if tick values drift from their expected values:
```c
#if (UART_FRAME_GAP_TICKS != 40)
#error "UART_FRAME_GAP_TICKS must be 40"
#endif
```

## 9. WCET and Jitter

### 9.1 Measurement Infrastructure

| Component | Status |
|-----------|--------|
| CPU Timer2 150MHz cycle counter | [x] Initialized in `Diagnostics_Init()` |
| `WcetSlot` (min/max/last) per measured context | [x] 3 slots: timer0_isr, sci_rx_isr, main_loop |
| Timer0 ISR WCET instrumentation | [x] Entry/exit cycle reads in `isr.c` |
| SCI RX ISR WCET instrumentation | [x] Both early-return and normal paths |
| Main loop WCET instrumentation | [x] Iteration-to-iteration latency in `main.c` |
| `g_diagnostics` placed in `diagnostics` section | [x] RAML4 @ 0xC2C0 |
| **Actual WCET values measured** | **[ ] 未验证** |

### 9.2 How to Measure

1. Load Debug build via JTAG, resume execution, let run for several seconds under load.
2. Pause, read `g_diagnostics` via JTAG memory browser at 0xC2C0.
3. Record `min_cycles` / `max_cycles` / `last_cycles` for each of the three slots.
4. Convert cycles to microseconds: `time_us = cycles / 150`.
5. Repeat under maximum load (continuous 64-byte UART frames at 4ms+ gaps).

### 9.3 Expected Rough Bounds (Unmeasured)

| Context | Expected WCET | Rationale |
|---------|--------------|-----------|
| Timer0 ISR | <1 μs | Tick increment + flag clear + PIE ack = ~100 cycles |
| SCI RX ISR (1 byte) | <5 μs | Status read + byte read + queue push = ~500 cycles |
| SCI RX ISR (16 bytes, FIFO full) | <50 μs | ~1500 cycles, dominated by 16× queue pushes |
| Main loop iteration (idle) | <2 μs | ~300 cycles: Timebase_Now + SpiBridge_Service (IDLE) + 3 scheduler checks |
| Main loop iteration (SPI active) | <10 μs | ~1500 cycles: queue drain + SPI state machine service |

These are order-of-magnitude estimates, not measurements. The actual values will be recorded here once JTAG readout is performed.

## 10. Long-Duration Stability

| Test | Status |
|------|--------|
| 8-24 hour continuous run | [ ] 未验证 |
| No deadlock / livelock | [ ] 未验证 (code review: no blocking loops, no mutexes, single-writer data model) |
| No counter overflow / wraparound bugs | [ ] 未验证 (code review: all time math uses uint32_t unsigned subtraction) |
| No memory corruption | [ ] 未验证 |
| Scheduler miss counters stable (non-growing) | [ ] 未验证 |

The architecture avoids the most common long-run failure modes:
- No dynamic allocation → no heap fragmentation.
- No recursion → stack depth bounded at compile time.
- Single-writer data model → no deadlock (no mutexes needed).
- `uint32_t` unsigned subtraction → wraparound-safe for ~49.7 days.
