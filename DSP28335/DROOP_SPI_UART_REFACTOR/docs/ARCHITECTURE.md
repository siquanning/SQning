# ARCHITECTURE — DROOP_SPI_UART_REFACTOR

## 1. Project Purpose

DROOP_SPI_UART_REFACTOR is a UART→SPI bridge firmware for the TMS320F28335 DSP. It receives variable-length UART frames over SCI-A, parses them into complete frames, and forwards each frame byte-by-byte over SPI-A to a CPLD slave. The project is a clean-layered refactor of the legacy `DROOP_SPI_UART` codebase.

**Target**: TMS320F28335, 150 MHz SYSCLKOUT, EABI (ELF) toolchain.  
**Compiler**: TI C2000 CGT 25.11.0.LTS (`cl2000 -v28`).  
**Builds**: RAM Debug (JTAG) + Flash Release (standalone boot).

## 2. Layer Architecture

```
                    ┌──────────────────────────────────┐
                    │  firmware/app/                    │
                    │  main.c  isr.c  scheduler.c       │
                    │  sci_rx_queue.c  app_context.c    │
                    │  diagnostics.c                    │
                    │  ───────────────────────────      │
                    │  Owns: main loop, ISR glue,       │
                    │        multi-rate scheduler,      │
                    │        SPSC queue, WCET           │
                    └────┬──────────┬──────────┬────────┘
                         │          │          │
              ┌──────────┘          │          └──────────────────────┐
              ▼                     ▼                                 ▼
   ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────────────┐
   │ firmware/        │  │ firmware/        │  │ firmware/control/        │
   │ services/        │  │ bsp/             │  │ (future)                 │
   │ spi_bridge.c     │  │ board.c          │  │ droop_control.c          │
   │ uart_frame.c     │  │ board_config.h   │  │ protection.c             │
   │ spi_request.c    │  │ board_pins.h     │  │                          │
   │ indicator.c      │  │                  │  │ Owns: control loops,     │
   │                  │  │ Owns: hardware   │  │        fault sequencing  │
   │ Owns: UART/SPI   │  │ assembly order   │  └────────────┬─────────────┘
   │ protocol layer   │  └────────┬─────────┘               │
   └────────┬─────────┘           │                         │
            │                     │                         ▼
            ▼                     ▼              ┌──────────────────────────┐
   ┌──────────────────────────────────────────┐ │ firmware/algorithm/      │
   │  firmware/drivers/                       │ │ (future)                 │
   │  drv_sysctrl.c  drv_timer.c  drv_sci.c   │ │ pid.c  filter.c         │
   │  drv_spi.c  drv_gpio.c  drv_interrupt.c  │ │                          │
   │  ────────────────────────────────────    │ │ Owns: pure computation   │
   │  Owns: ALL peripheral register access    │ │        no HW dependency   │
   └────────────────────┬─────────────────────┘ └──────────────────────────┘
                        │
                        ▼
   ┌──────────────────────────────────────────┐
   │  TI Register Headers + RTS Library       │
   │  DSP2833x_*.h  DSP2833x_Device.h         │
   │  SRC/ (TI-provided .c/.asm files)        │
   └──────────────────────────────────────────┘
```

### 2.1 Dependency Rule

Three parallel dependency chains, all rooted at `app/`:

```
app ──► services ──► drivers ──► TI
app ──► bsp      ──► drivers ──► TI
app ──► control  ──► algorithm  (pure, no hardware)
```

Key constraints:
- **app/** and **services/** must never include `DSP2833x_*.h` or access peripheral registers.
- **drivers/** must never include service headers (`uart_frame.h`, `spi_request.h`, etc.).
- **bsp/** calls drivers but contains no business logic — it only wires hardware assembly order.
- **bsp/** does NOT sit "below" drivers in a dependency sense; both services and bsp call drivers directly. The linear stack `App → Services → Drivers → BSP → TI` is incorrect.
- **control/** and **algorithm/** are future layers. When implemented, control owns task allocation and sequencing; algorithm is pure computation with context structs, host-testable with zero hardware dependencies.

## 3. Module Inventory

### 3.1 App Layer

| Module | Files | Responsibility |
|--------|-------|---------------|
| **main** | `main.c` | Static root `g_app`, `g_sched`; infinite loop: drain queue → service bridge → indicator → scheduler |
| **isr** | `isr.c` `.h` | Two ISRs: Timer0 (tick advance→ack group1), SCI-A RX (bytes→queue→ack group9); no service-layer calls |
| **scheduler** | `scheduler.c` `.h` | Multi-rate periodic task dispatcher: 1ms/10ms/100ms slots; miss counter per slot; wraparound-safe via `uint32_t` unsigned subtraction |
| **sci_rx_queue** | `sci_rx_queue.c` `.h` | Lock-free SPSC ring buffer (129 slots, 128 effective); ISR pushes, main loop pops; overflow counter |
| **app_context** | `app_context.c` `.h` | Top-level context: SciRxQueue + SpiBridgeContext; `AppContext_Init()` chains sub-init |
| **diagnostics** | `diagnostics.c` `.h` | CPU Timer2 free-running 150MHz cycle counter; WCET min/max/last per slot; snapshot at 100ms |

### 3.2 Services Layer

| Module | Files | Responsibility |
|--------|-------|---------------|
| **spi_bridge** | `spi_bridge.c` `.h` | Owns UartFrameContext + SpiRequestContext; `OnRxByte` feeds parser, `Service()` drives SPI state machine; `frames_forwarded` / `spi_timeouts` counters |
| **uart_frame** | `uart_frame.c` `.h` | UART protocol: IDLE→RECEIVING→READY/TOO_LONG state machine; 4ms inter-byte gap detection; 64-byte capacity limit; busy-drop on unconsumed ready frame |
| **spi_request** | `spi_request.c` `.h` | SPI byte-level state machine: IDLE→WAIT_GAP→WAIT_DONE→DONE/TIMEOUT; 1ms inter-byte gap; 5ms single-byte timeout; MISO checking (0xFF=idle) |
| **indicator** | `indicator.c` `.h` | LED state machine: `TriggerRx`/`TriggerTx` → 500-tick (50ms) active pulse; `Service()` handles decay |

### 3.3 Drivers Layer

| Module | Files | Responsibility |
|--------|-------|---------------|
| **drv_sysctrl** | `drv_sysctrl.c` `.h` | PLL configuration, DIVSEL, HISPCP/LOSPCP, peripheral clock gating, watchdog disable; `DrvFlash_Init()` in ramfuncs |
| **drv_timer** | `drv_timer.c` `.h` | CPU Timer0 init/start, 100μs tick counter, `Timebase_Now()` with double-read safety for 32-bit on 16-bit bus |
| **drv_sci** | `drv_sci.c` `.h` | SCI-A FIFO RX, error detection, byte read, error recovery, interrupt flag clear |
| **drv_spi** | `drv_spi.c` `.h` | SPI-A master, `StartByte()` (non-blocking TX), `TryCompleteByte()` (poll RX ready) |
| **drv_gpio** | `drv_gpio.c` `.h` | Output pin set/clear/init; used for LED GPIO67/68 |
| **drv_interrupt** | `drv_interrupt.c` `.h` | PIE vector table init, ISR binding (Timer0→INT1.0, SCI-A RX→INT9.1), global interrupt enable |

### 3.4 BSP Layer

| Module | Files | Responsibility |
|--------|-------|---------------|
| **board** | `board.c` `.h` | Hardware assembly: clock→MemCopy(ramfuncs)→Flash→Timer0→SCI→SPI→interrupt enable |
| **board_config** | `board_config.h` | Numeric constants: SYSCLK=150MHz, LSPCLK=37.5MHz, BAUD=9600, SPIBRR=127, TIMER0=100μs |
| **board_pins** | `board_pins.h` | GPIO pin assignments: SCI(35,36), SPI(16,17,18), LED(67,68) |

## 4. Data Ownership

### 4.1 Single-Writer Principle

Every field has exactly one writer context:

| Data | Writer | Readers |
|------|--------|---------|
| `g_sysTick` (uint32_t) | Timer0 ISR | Main loop (via `Timebase_Now()`) |
| `SciRxQueue.items[]` | SCI-A RX ISR (write_index only) | Main loop (read_index only) |
| `SciRxQueue.write_index` | SCI-A RX ISR | Main loop |
| `SciRxQueue.read_index` | Main loop | SCI-A RX ISR |
| `SciRxQueue.overflow_count` | SCI-A RX ISR (volatile) | Main loop (100ms snapshot) |
| `UartFrameContext.*` | Main loop (via SpiBridge) | Diagnostics snapshot |
| `SpiRequestContext.*` | Main loop (via SpiBridge) | Diagnostics snapshot |
| `Scheduler.{last_*, miss_*}` | Main loop | 100ms snapshot |
| `Diagnostics.*` | ISRs + Main loop (WcetUpdate) | 100ms snapshot |

### 4.2 Volatile Qualifiers

`volatile` is used ONLY on SPSC queue members (`write_index`, `read_index`, `overflow_count`) that are written by an ISR and read by the main loop. All other shared state is protected by the single-threaded main loop — ISR writes go into the queue, the main loop drains it.

## 5. ISR Design

### 5.1 Timer0 ISR (INT1.0, every 100μs)

```
App_Timer0Isr:
  1. Read cycle counter (WCET start)
  2. DrvTimer0_OnInterrupt() — advance g_sysTick, clear flag
  3. DrvInterrupt_AckGroup1() — PIE acknowledge
  4. WCET update (stop - start)
```

### 5.2 SCI-A RX ISR (INT9.1, triggered at ≥1 byte in FIFO)

```
App_SciaRxIsr:
  1. Read cycle counter (WCET start)
  2. Read Timebase_Now() for item timestamp
  3. NULL-guard on queue pointer
  4. If error flags set → push error item
  5. Else:
     a. Read FIFO count
     b. For each byte in FIFO:
        - Check RX status for errors
        - If error → push error item, break
        - Read byte → push data item
  6. If any error → DrvSci_RecoverRx()
  7. DrvSci_ClearRxInterrupt() + DrvInterrupt_AckGroup9()
  8. WCET update
```

### 5.3 ISR Constraints

- No service-layer calls (no `UartFrame_*`, no `SpiBridge_*`, no `Indicator_*`).
- Only writes to `SciRxQueue` (lock-free SPSC, no disable-interrupts needed).
- reads `Timebase_Now()` once at entry — all items in a batch share the same timestamp.
- WCET instrumentation is minimal: two `CpuTimer2Regs.TIM.all` reads, one `WcetUpdate()` call.

## 6. Main Loop & Scheduler

### 6.1 Main Loop (unconditional, runs forever)

```
for (;;)
{
    now = Timebase_Now();

    // 1. Drain SCI RX queue into SpiBridge
    while (SciRxQueue_Pop(&item))
        SpiBridge_OnRxByte() or SpiBridge_OnRxError()

    // 2. Service SPI bridge state machine (non-blocking)
    SpiBridge_Service(now, DrvSpi_StartByte, DrvSpi_TryCompleteByte)

    // 3. Service LED indicator (non-blocking)
    Indicator_Service(now)

    // 4. Multi-rate scheduler tasks
    if Scheduler_Take1ms(now)   → (reserved)
    if Scheduler_Take10ms(now)  → (reserved)
    if Scheduler_Take100ms(now) → snapshot diagnostics
}
```

### 6.2 Scheduler Design

| Period | Ticks | Miss Detection | Current Use |
|--------|-------|----------------|-------------|
| 1ms | 10 | `elapsed ≥ 20` → miss++, resync to now | Reserved |
| 10ms | 100 | `elapsed ≥ 200` → miss++, resync to now | Reserved |
| 100ms | 1000 | `elapsed ≥ 2000` → miss++, resync to now | Diagnostics snapshot |

The scheduler uses `uint32_t` unsigned subtraction (`elapsed = now - last`) which is wraparound-safe for the ~49.7-day tick counter period. Miss detection uses `elapsed >= period * 2` — if more than one full period has elapsed, a miss is counted and the slot resyncs to `now` (rather than accumulating catch-up calls).

### 6.3 Non-Blocking Guarantee

Every function called in the main loop returns immediately. There are no `while`-spin loops (except the infinite outer loop), no `delay_ms()`, no blocking SPI waits. SPI byte completion is polled via `TryCompleteByte()` which checks `SPIFFRX.RXFFST` and returns immediately if no byte is ready.

## 7. UART→SPI Data Flow

```
SCI-A RX pin (GPIO36)
    │
    ▼
SCI-A RX ISR (App_SciaRxIsr)
    │  DrvSci_ReadByte(), DrvSci_GetRxFifoCount()
    │  SciRxQueue_PushFromIsr(data, error_flags, now)
    ▼
SciRxQueue (129-slot ring buffer, lock-free SPSC)
    │
    ▼
Main Loop: SciRxQueue_Pop()
    │
    ▼
SpiBridge_OnRxByte(data, tick)
    │
    ▼
UartFrame_OnByte() — frame assembly
    │  IDLE → RECEIVING → READY (gap ≥ 4ms) or TOO_LONG (>64 bytes)
    │
    ▼ (on READY)
SpiRequest_Start(data, length, tick) — begin SPI transfer
    │
    ▼
SpiBridge_Service() — per-iteration poll
    │
    ├── SpiRequest_Service() — byte-by-byte state machine
    │     ├── IDLE → WAIT_GAP (1ms)
    │     ├── WAIT_GAP → WAIT_DONE (DrvSpi_StartByte)
    │     ├── WAIT_DONE → WAIT_GAP (DrvSpi_TryCompleteByte)
    │     └── DONE / TIMEOUT (5ms)
    │
    ▼
SPI-A MOSI pin (GPIO16)
    │
    ▼
CPLD Slave (MISO → GPIO17)
```

### 7.1 Timing Budget

| Phase | Duration | Margin |
|-------|----------|--------|
| SCI byte receipt (9600 bps) | ~1.04 ms/byte | — |
| ISR → queue push | ~1 μs | — |
| Frame gap detection | 4 ms idle | 3.9× byte-time |
| SPI byte transfer (~293 kHz) | ~34 μs/byte | — |
| SPI inter-byte gap | 1 ms | 29× byte-time |
| SPI single-byte timeout | 5 ms | 147× byte-time |
| Full 64-byte frame forwarding | ~66 ms | — |

### 7.2 Busy-Drop Policy

If a UART frame is READY but the SPI bridge is still transferring the previous frame, new incoming bytes trigger `busy_drops`. This is intentional: there is a single frame buffer (not a queue of frames). The caller must ensure inter-frame gaps exceed the SPI transfer time (~66ms for 64 bytes).

## 8. Diagnostics & WCET

### 8.1 CPU Timer2 Cycle Counter

CPU Timer2 runs as a free-running 32-bit down-counter at SYSCLKOUT (150 MHz):

- Resolution: 6.67 ns/tick
- Range: ~28.6 seconds before wraparound
- Usage: `Diagnostics_CycleRead()` → `CpuTimer2Regs.TIM.all`
- Measurement: `elapsed = start - stop` (down-counter, unsigned subtraction is wraparound-safe)

### 8.2 WCET Slots

| Slot | Measured In | Update Frequency |
|------|-------------|-----------------|
| `timer0_isr` | Timer0 ISR | Every 100 μs |
| `sci_rx_isr` | SCI-A RX ISR | Per RX interrupt (variable) |
| `main_loop` | Main loop start | Every iteration |

Each slot records `min_cycles`, `max_cycles`, `last_cycles`. The ISR measurements include everything from ISR entry to just before the final `WcetUpdate()` call. The main loop measurement captures one full iteration (including UART/SPI service and scheduler checks).

### 8.3 Diagnostics Snapshot (100ms)

Once per 100ms, the main loop snapshots:
- Scheduler miss counters (`miss_1ms`, `miss_10ms`, `miss_100ms`)
- SCI RX queue overflow count
- These fields are part of the `Diagnostics` struct in the `diagnostics` linker section.

## 9. Memory Layout

See [MEMORY_LAYOUT.md](MEMORY_LAYOUT.md) for the complete memory map. Key points:

### 9.1 RAM Debug

| Section | Placement | Size |
|---------|-----------|------|
| .text, .cinit | RAML03 (0x8000) | ~3.3K words |
| .stack | RAMM1 (0x0400) | 0x300 (768 bytes) |
| .bss, .data, comm_buffer, diagnostics | RAML4 (0xC000) | ~1.0K words |
| .const | RAML5 (0xD000) | ~0.3K words |

### 9.2 Flash Release

| Section | Load (Flash) | Run (RAM) |
|---------|-------------|-----------|
| .text, .cinit, .switch | FLASHB (0x330000) | FLASHB |
| codestart | BEGIN (0x33FFF6) | BEGIN |
| ramfuncs | FLASHB | RAML03 (0x8000) |
| .bss, .data, comm_buffer, diagnostics | — | RAML4 |
| .const | — | RAML5 |
| .stack | — | RAMM1 |

### 9.3 Semantic Sections

| Section | Placement | Owner | Purpose |
|---------|-----------|-------|---------|
| `comm_buffer` | RAML4 | SciRxQueue items[] | ISR↔main SPSC buffer, DMA-accessible for future |
| `diagnostics` | RAML4 | Diagnostics struct | WCET + miss + overflow snapshot |
| `ramfuncs` | FLASHB→RAML03 | DrvFlash_Init, DSP28x_usDelay | Must execute from RAM (Flash reconfig, timing) |

## 10. Build Configurations

### 10.1 RAM Debug (CCS IDE)

- **Linker**: `linker/28335_RAM_lnk.cmd`
- **Define**: none (no `FLASH` symbol)
- **Load**: JTAG (XDS100v3)
- **Use**: Development, debugging, register inspection

### 10.2 Flash Release (PowerShell build script)

- **Linker**: `linker/f28335_flash.cmd`
- **Define**: `FLASH` (enables `MemCopy()` + `DrvFlash_Init()` in `Board_Init()`)
- **Build**: `Release/build.ps1`
- **Load**: JTAG flash programmer or serial bootloader
- **Boot**: Standalone (power-on → Boot ROM → BEGIN (0x33FFF6) → codestart → _c_int00 → main)

### 10.3 Flash Boot Sequence

1. Boot ROM samples GPIO pins, selects Flash boot mode
2. Jumps to 0x33FFF6 (BEGIN)
3. `code_start`: disable watchdog, branch to `_c_int00`
4. `_c_int00` (RTS): copy .cinit to .bss/.data, zero-init .bss
5. `main()`: `Board_Init()`
6. `MemCopy()`: copy ramfuncs from Flash to RAML03 (while Flash at default wait states)
7. `DrvSysCtrl_Init()`: PLL lock, clock tree
8. `DrvFlash_Init()`: configure Flash wait states + pipeline (executes from RAM)
9. Peripheral init: Timer0, SCI-A, SPI-A, interrupts
10. `DrvInterrupt_EnableGlobal()` + `DrvTimer0_Start()` → main loop

## 11. Host Test Architecture

PC-host tests validate pure-logic modules without DSP hardware:

| Test Suite | Module Under Test | Tests | Focus |
|------------|-------------------|-------|-------|
| `test_uart_frame` | uart_frame.c | 9 | Frame assembly, gap detection, overlong, busy-drop, wraparound |
| `test_spi_request` | spi_request.c | 14 | Byte state machine, gap/timeout, MISO checking, edge cases |
| `test_sci_rx_queue` | sci_rx_queue.c | 6 | SPSC push/pop, full/empty, overflow, wraparound |
| `test_spi_bridge` | spi_bridge.c | 10 | OnRxByte→frame→SPI forwarding, OnRxError, Service orchestration |

All 39 tests run on Windows x64 (MSVC). DSP target compilation produces a single placeholder variable via `#ifdef __TMS320C28XX__` guard — no code executes on the target.

## 12. Static Invariants (Enforced by Grep)

| # | Rule | Scope | Current |
|---|------|-------|---------|
| 1 | No `DSP2833x_*` includes | services/, app/ | 0 matches |
| 2 | No `Regs\|EALLOW\|EDIS` | services/, app/ | 0 matches |
| 3 | No service includes | drivers/ | 0 matches |
| 4 | No TI headers | services/ | 0 matches |
| 5 | No `extern` writable globals | services/, app/ | Only linker symbols in board.c |
| 6 | `volatile` only on SPSC indices | entire firmware | write_index, read_index, overflow_count |

## 13. Directory Map

```
DROOP_SPI_UART_REFACTOR/
├── firmware/
│   ├── app/           main, isr, scheduler, sci_rx_queue, app_context, diagnostics
│   ├── services/      spi_bridge, uart_frame, spi_request, indicator
│   ├── drivers/       drv_sysctrl, drv_timer, drv_sci, drv_spi, drv_gpio, drv_interrupt
│   └── bsp/           board, board_config, board_pins
├── config/            comm_config.h (timing constants, compile-time checks)
├── linker/            28335_RAM_lnk.cmd (Debug), f28335_flash.cmd (Release)
├── INCLUDE/           TI DSP2833x register headers
├── SRC/               TI-provided library sources (.c/.asm)
├── CMD/               DSP2833x_Headers_nonBIOS.cmd (peripheral register placement)
├── Release/           build.ps1, output .out/.map
├── tests/
│   ├── host/          4 test suites + MSVC build scripts (39 tests)
│   └── hardware/      BASELINE_TEST_RECORD.md + baseline artifacts
└── docs/              ARCHITECTURE.md, TASK_TIMING.md, MEMORY_LAYOUT.md,
                        HARDWARE_TEST.md, BUILD_AND_FLASH.md,
                        ALGORITHM_CONTRACT.md, 四步重构实施方案.md
```

## 14. Future Extension Points

These are designed but not yet implemented:

| Feature | Slot | Status |
|---------|------|--------|
| Fast control (PID, current loop) | 1ms scheduler task | Reserved, no code |
| Droop control outer loop | 10ms scheduler task | Reserved, no code |
| Protection / fault handling | 10ms scheduler task | Reserved, no code |
| ADC + DMA buffers | L6/L7 SARAM | Memory reserved, no code |
| Algorithm modules (PID, filter) | `firmware/algorithm/` | Contract defined (ALGORITHM_CONTRACT.md), no modules |
| Indicator LED callers | `Indicator_TriggerRx/Tx` | API exists, no callers wired |
| SPI hardware fault injection test | Hardware test | Not yet executed |

For algorithm module requirements, see [ALGORITHM_CONTRACT.md](ALGORITHM_CONTRACT.md).
