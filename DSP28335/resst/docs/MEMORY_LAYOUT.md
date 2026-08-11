# MEMORY_LAYOUT — F28335_RTControl_Platform

## 1. F28335 Memory Overview

| Block | Address | Size | Type | Notes |
|-------|---------|------|------|-------|
| M0 SARAM | 0x0000-0x03FF | 1K×16 | RAM | BOOT_RSVD(0x00-0x4F), M0(0x50-), BEGIN |
| M1 SARAM | 0x0400-0x07FF | 1K×16 | RAM | Stack (.stack) |
| L0 SARAM | 0x8000-0x8FFF | 4K×16 | RAM | Code (.text, .cinit) |
| L1 SARAM | 0x9000-0x9FFF | 4K×16 | RAM | Part of RAML03 combined |
| L2 SARAM | 0xA000-0xAFFF | 4K×16 | RAM | Part of RAML03 combined |
| L3 SARAM | 0xB000-0xBFFF | 4K×16 | RAM | Part of RAML03 combined |
| L4 SARAM | 0xC000-0xCFFF | 4K×16 | RAM | .bss, .data |
| L5 SARAM | 0xD000-0xDFFF | 4K×16 | RAM | .const |
| L6 SARAM | 0xE000-0xEFFF | 4K×16 | RAM | Unused |
| L7 SARAM | 0xF000-0xFFFF | 4K×16 | RAM | Unused |
| Flash A-H | 0x300000-0x33FFFF | 256K×16 | Flash | See Flash layout §5 |
| Boot ROM | 0x3FE000-0x3FFFFF | 8K×16 | ROM | IQ/FPU tables + bootloader |

F28335 SARAM is single-access, no cache. All SARAM blocks have identical access time (0 wait states).
DMA can access L4-L7 SARAM (0xC000-0xFFFF). M0/M1 are also DMA-accessible.
L0-L3 SARAM are NOT DMA-accessible.

## 2. RAM Debug Configuration (linker/28335_RAM_lnk.cmd)

### 2.1 Segment Placement

| Section | Page | Memory | Address Range | Size | Used |
|---------|------|--------|---------------|------|------|
| .text | 0 | RAML03 | 0x8000-0x8CAA | 3242 | 20.4% |
| .cinit | 0 | RAML03 | 0x8CAA-0x8D16 | 108 | 0.7% |
| .stack | 1 | RAMM1 | 0x0400-0x0700 | 768 | 75.0% |
| .bss | 1 | RAML4 | 0xC000-0xC2A0 | 672 | 16.4% |
| .data | 1 | RAML4 | 0xC2A0-0xC2AC | 12 | 0.3% |
| .const | 1 | RAML5 | 0xD000-0xD10A | 266 | 6.5% |

### 2.2 Module .bss Breakdown

| Module | .bss Size | Contents |
|--------|-----------|----------|
| main.obj | 640 bytes | AppContext (SciRxQueue + SpiBridgeContext), Scheduler |
| DSP2833x_CpuTimers.obj | 24 bytes | CpuTimer0/1/2 structs |
| indicator.obj | 6 bytes | Indicator context |
| drv_timer.obj | 2 bytes | g_sysTick |

### 2.3 Module .text Breakdown (Code)

| Module | Code Size | Function |
|--------|-----------|----------|
| DSP2833x_DefaultIsr.obj | 803 | Default ISR stubs |
| uart_frame.obj | 318 | UART frame parser |
| spi_request.obj | 285 | SPI byte state machine |
| sci_rx_queue.obj | 193 | SPSC queue |
| drv_sci.obj | 174 | SCI driver |
| isr.obj | 181 | ISR entry points |
| spi_bridge.obj | 137 | UART→SPI bridge |
| scheduler.obj | 134 | Multi-rate scheduler |
| DSP2833x_CpuTimers.obj | 123 | Timer config |
| drv_spi.obj | 98 | SPI driver |
| drv_sysctrl.obj | 85 | Clock init |
| main.obj | 78 | Main loop |
| indicator.obj | 74 | LED indicator |
| drv_gpio.obj | 71 | GPIO driver |
| board.obj | 62 | Board init |
| drv_interrupt.obj | 61 | PIE/vector init |
| drv_timer.obj | 45 | Timer driver + Timebase_Now |
| app_context.obj | 12 | Context init |
| RTS library | 236 | Boot, memcpy, autoinit, etc. |

### 2.4 Capacity Summary

| Memory | Total | Used | Free | Utilization |
|--------|-------|------|------|-------------|
| RAML03 (code) | 16384 | 3350 | 13034 | 20.4% |
| RAMM1 (stack) | 1024 | 768 | 256 | 75.0% |
| RAML4 (data) | 4096 | 684 | 3412 | 16.7% |
| RAML5 (const) | 4096 | 266 | 3830 | 6.5% |
| RAML6 | 4096 | 0 | 4096 | 0% |
| RAML7 | 4096 | 0 | 4096 | 0% |

### 2.5 Stack

- Size: 0x300 (768 bytes)
- Location: RAMM1 0x0400-0x0700
- No stack overflow detection in hardware
- Recommended minimum free margin: 256 bytes

## 3. Semantic Memory Sections

### 3.1 comm_buffer — Communication Buffers

Placed in RAML4 for DMA accessibility (future-proofing).

```c
#pragma DATA_SECTION(g_app.sci_rx_queue.items, "comm_buffer")
```

| Property | Value |
|----------|-------|
| Segment | comm_buffer |
| Placement | RAML4 (PAGE 1) |
| Address | 0xC000 |
| Used | ~1032 bytes (129 × SciRxItem) |
| Capacity | 4096 bytes (RAML4) |
| Owner | SciRxQueue (ISR writes, main reads) |

The SciRxQueue items array is the largest single data allocation (~1032 bytes).
It is the primary communication buffer between the SCI ISR and the main loop.
Placing it in a named section documents its purpose and enables future relocation
(e.g., to DMA-accessible RAML6/L7 if needed for future ADC/DMA buffers).

Explicit `#pragma` is applied only to the items[] array — NOT the entire AppContext
struct — to keep the semantic intent clear without mechanically forcing all globals
into special sections.

### 3.2 diagnostics — Diagnostic Snapshots

Placed in RAML4 near the comm data for coherent access patterns.

No `#pragma` is needed for the diagnostic fields — they are already part of
UartFrameContext and SpiRequestContext within AppContext, naturally grouped
and accessible via snapshot functions (`UartFrame_GetDiagnostics`,
`SpiRequest_GetDiagnostics`, `Scheduler_GetDiagnostics`).

### 3.3 Sections NOT Created (Reserved for Future)

The following sections from the plan are deliberately NOT yet created:

| Section | Reason |
|---------|--------|
| fastcode | No high-frequency ISR or time-critical control code currently exists |
| fastdata | No fast control state exists |
| adc_buffer | No ADC or DMA active; creating empty buffers wastes RAM and misleads |

These will be created when the corresponding control/ADC/DMA modules are actually
implemented and profiled.

## 4. Memory Safety Rules

1. No dynamic allocation — all buffers are statically sized at link time.
2. Stack overflow: no hardware detection; 256-byte margin maintained.
3. All shared data has a single owner (ISR or main loop), documented per field.
4. `volatile` used only on SPSC queue indices (write_index, read_index, overflow_count).
5. No dual-mapped memory regions (PAGE 0 and PAGE 1 never overlap).
6. All TI peripheral register files placed by DSP2833x_Headers_nonBIOS.cmd.

## 5. Flash Layout (Flash_Release: linker/f28335_flash.cmd)

### 5.1 F28335 Flash Sectors

| Sector | Address Range | Size (×16) |
|--------|---------------|------------|
| H | 0x300000-0x307FFF | 32K |
| G | 0x308000-0x30FFFF | 32K |
| F | 0x310000-0x317FFF | 32K |
| D | 0x320000-0x327FFF | 32K |
| C | 0x328000-0x32FFFF | 32K |
| B | 0x330000-0x337FFF | 32K |
| A | 0x338000-0x33FF7F | 32768-128 |
| CSM_RSVD | 0x33FF80-0x33FFF5 | 118 |
| BEGIN | 0x33FFF6-0x33FFF7 | 2 |
| CSM_PWL | 0x33FFF8-0x33FFFF | 8 |

### 5.2 Flash Release Segment Placement

| Section | Load (Flash) | Run (RAM) |
|---------|-------------|-----------|
| .text | FLASHB (0x330000) | FLASHB |
| .cinit | FLASHB | — |
| codestart | BEGIN (0x33FFF6) | BEGIN |
| ramfuncs | FLASHB | RAML03 (0x8000) |
| .stack | — | RAMM1 |
| .bss | — | RAML4 |
| .data | — | RAML4 |
| .const | — | RAML5 |
| .switch | FLASHB | FLASHB |
| .init_array | FLASHB | FLASHB |

### 5.3 Flash Initialization Sequence

1. Boot ROM jumps to 0x33FFF6 (codestart)
2. codestart → wd_disable → _c_int00 (RTS boot)
3. _c_int00 → C init (copy .cinit to .bss/.data, zero-init)
4. main() called
5. Board_Init() → DrvSysCtrl_Init (PLL, clocks)
6. InitFlash() called from RAM (ramfuncs)
7. MemCopy ramfuncs from Flash to RAM
8. Peripheral init (Timer, SCI, SPI, interrupts)

The Flash wait-state and pipeline configuration code (InitFlash) MUST run from
RAM because the Flash is being reconfigured. The TI-provided InitFlash() in
DSP2833x_SysCtrl.c handles this — it is placed in the ramfuncs section via:
```c
#pragma CODE_SECTION(InitFlash, "ramfuncs");
```

### 5.4 Flash Wait States (150 MHz SYSCLKOUT)

| Register | Value | Meaning |
|----------|-------|---------|
| FOPT | 0x000F | Enable pipeline mode |
| FPWR | 0x0003 | Flash bank/ pump powers to active |
| FSTDBYWAIT | 0x01FF | Sleep to standby: 511 SYSCLKOUT cycles |
| FACTIVEWAIT | 0x01FF | Standby to active: 511 SYSCLKOUT cycles |
| FBANKWAIT | 0x0005 | Random access: 5 wait states |
| FBANKWAIT | 0x0005 | Paged access: 5 wait states |
| FOTPWAIT | 0x0008 | OTP access: 8 wait states |

These are the TI-recommended values for 150 MHz. See DSP2833x_SysCtrl.c InitFlash().

## 6. DMA Accessibility (Future Reference)

When ADC/DMA buffers are added:

| SARAM Block | Address | DMA Accessible |
|-------------|---------|----------------|
| M0 | 0x0000-0x03FF | Yes |
| M1 | 0x0400-0x07FF | Yes |
| L0-L3 | 0x8000-0xBFFF | No |
| L4-L7 | 0xC000-0xFFFF | Yes |

DMA buffers must be placed in L4-L7 (or M0/M1), NOT in L0-L3.
The DMA has a 16-bit address reach, so all internal SARAM is accessible.
Alignment requirements: DMA transfers must be 32-bit aligned for 32-bit transfers.
