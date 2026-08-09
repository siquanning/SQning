#include "DSP2833x_Device.h"
#include "firmware/app/diagnostics.h"

#pragma DATA_SECTION(g_diagnostics, "diagnostics")
static Diagnostics g_diagnostics;

void Diagnostics_Init(void)
{
    /* CPU Timer2: free-running down-counter at SYSCLKOUT (150 MHz).
     * No interrupt — just a high-resolution time source for WCET. */
    CpuTimer2Regs.TPR.all  = 0x0000;
    CpuTimer2Regs.PRD.all  = 0xFFFFFFFF;
    CpuTimer2Regs.TCR.bit.TRB = 1;
    CpuTimer2Regs.TCR.bit.TSS = 0;

    g_diagnostics.timer0_isr.min_cycles = 0xFFFFFFFF;
    g_diagnostics.sci_rx_isr.min_cycles = 0xFFFFFFFF;
    g_diagnostics.main_loop.min_cycles  = 0xFFFFFFFF;
    g_diagnostics.timer0_isr.max_cycles = 0;
    g_diagnostics.sci_rx_isr.max_cycles = 0;
    g_diagnostics.main_loop.max_cycles  = 0;
    g_diagnostics.timer0_isr.last_cycles = 0;
    g_diagnostics.sci_rx_isr.last_cycles = 0;
    g_diagnostics.main_loop.last_cycles  = 0;
    g_diagnostics.miss_1ms   = 0;
    g_diagnostics.miss_10ms  = 0;
    g_diagnostics.miss_100ms = 0;
    g_diagnostics.sci_rx_overflow = 0;
    g_diagnostics.sci_rx_total    = 0;
}

Diagnostics *Diagnostics_Get(void)
{
    return &g_diagnostics;
}

uint32_t Diagnostics_CycleRead(void)
{
    return CpuTimer2Regs.TIM.all;
}

void Diagnostics_WcetUpdate(WcetSlot *slot, uint32_t elapsed_cycles)
{
    if (elapsed_cycles > slot->max_cycles)
        slot->max_cycles = elapsed_cycles;
    if (elapsed_cycles < slot->min_cycles)
        slot->min_cycles = elapsed_cycles;
    slot->last_cycles = elapsed_cycles;
}
