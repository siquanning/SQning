#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <stdint.h>

typedef struct
{
    uint32_t min_cycles;
    uint32_t max_cycles;
    uint32_t last_cycles;
} WcetSlot;

typedef struct
{
    WcetSlot timer0_isr;
    WcetSlot sci_rx_isr;
    WcetSlot main_loop;
    uint32_t miss_1ms;
    uint32_t miss_10ms;
    uint32_t miss_100ms;
    uint32_t sci_rx_overflow;
    uint32_t sci_rx_total;
} Diagnostics;

/*
 * Initialise CPU Timer2 as a free-running 32-bit down-counter at SYSCLKOUT
 * (150 MHz, ~28.6 s wraparound) and reset all WCET slots.
 * Must be called after DrvSysCtrl_Init() so the clock is stable.
 */
void Diagnostics_Init(void);

/* Get the global diagnostics instance (placed in "diagnostics" section). */
Diagnostics *Diagnostics_Get(void);

/*
 * Read the free-running counter.  Counts down — use (start - stop) for elapsed.
 * At 150 MHz each tick is ~6.67 ns.  Range: ~28.6 seconds before wraparound.
 */
uint32_t Diagnostics_CycleRead(void);

/* Update a WCET slot with a new elapsed measurement. */
void Diagnostics_WcetUpdate(WcetSlot *slot, uint32_t elapsed_cycles);

#endif
