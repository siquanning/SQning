# reference/legacy

Historical reference code from the original `DROOP_SPI_UART` monolithic implementation. These files are **excluded from all active builds** and preserved solely for migration reference.

## Contents

| Original Location | Legacy File | Replaced By |
|---|---|---|
| `firmware/board/` | `board.c`, `board.h` | `firmware/bsp/board.c`, `firmware/drivers/drv_*.c`, `firmware/app/isr.c` |
| `firmware/comm/` | `led.c`, `led.h` | `firmware/services/indicator.c` |

## Migration Notes

- `board.c` was a monolithic file containing SCI init, SPI init, Timer0 init, and ISRs in one translation unit.
- The refactored architecture splits these into: BSP (board init), Drivers (per-peripheral register access), and App/ISR (interrupt handlers).
- `led.c` directly accessed `GpioDataRegs` and `GpioCtrlRegs`. The replacement `indicator.c` uses `DrvGpio_*` abstraction with `board_pins.h` pin definitions.

These files should NOT be included in any build configuration. They exist only to help understand the migration from the original codebase.
