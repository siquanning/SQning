/* Created by Siquanning */
#include "firmware/app/diagnostics.h"
#include "firmware/drivers/drv_timer.h"

#pragma DATA_SECTION(g_diagnostics, "diagnostics")
static Diagnostics g_diagnostics;

void Diagnostics_Init(void)
{
    /* Delegate Timer2 cycle-counter init to the driver layer.
     * CPU Timer2: free-running down-counter at SYSCLKOUT (Profile-derived).
     * No interrupt — just a high-resolution time source for WCET. */
    DrvTimer2_CycleInit();

    g_diagnostics.timer0_isr.min_cycles = 0xFFFFFFFF;
    g_diagnostics.sci_rx_isr.min_cycles = 0xFFFFFFFF;
    g_diagnostics.fast_isr.min_cycles   = 0xFFFFFFFF;
    g_diagnostics.adc_isr.min_cycles    = 0xFFFFFFFF;
    g_diagnostics.main_loop.min_cycles  = 0xFFFFFFFF;
    g_diagnostics.timer0_isr.max_cycles = 0;
    g_diagnostics.sci_rx_isr.max_cycles = 0;
    g_diagnostics.fast_isr.max_cycles   = 0;
    g_diagnostics.adc_isr.max_cycles    = 0;
    g_diagnostics.main_loop.max_cycles  = 0;
    g_diagnostics.timer0_isr.last_cycles = 0;
    g_diagnostics.sci_rx_isr.last_cycles = 0;
    g_diagnostics.fast_isr.last_cycles   = 0;
    g_diagnostics.adc_isr.last_cycles    = 0;
    g_diagnostics.main_loop.last_cycles  = 0;
    g_diagnostics.miss_1ms   = 0;
    g_diagnostics.miss_10ms  = 0;
    g_diagnostics.miss_100ms = 0;
    g_diagnostics.sci_rx_overflow = 0;
    g_diagnostics.sci_rx_total    = 0;
    g_diagnostics.fast_isr_count  = 0;
    g_diagnostics.adc_isr_count   = 0;
    g_diagnostics.adc_raw[0]      = 0;
    g_diagnostics.adc_raw[1]      = 0;
    g_diagnostics.trip_flags      = 0;
    g_diagnostics.pwm_period      = 0;
    g_diagnostics.pwm_counter     = 0;

    g_diagnostics.system_state    = 0;
    g_diagnostics.fault_code      = 0;
    g_diagnostics.fault_tick      = 0UL;
    g_diagnostics.param_commit_count    = 0UL;
    g_diagnostics.param_reject_count    = 0UL;
    g_diagnostics.param_last_reject_reason = 0U;
    g_diagnostics.telem_overrun_count    = 0UL;
    g_diagnostics.telem_write_count     = 0UL;
    g_diagnostics.diag_flags            = 0UL;
}

Diagnostics *Diagnostics_Get(void)
{
    return &g_diagnostics;
}

uint32_t Diagnostics_CycleRead(void)
{
    return DrvTimer2_CycleRead();
}

void Diagnostics_WcetUpdate(WcetSlot *slot, uint32_t elapsed_cycles)
{
    if (elapsed_cycles > slot->max_cycles)
        slot->max_cycles = elapsed_cycles;
    if (elapsed_cycles < slot->min_cycles)
        slot->min_cycles = elapsed_cycles;
    slot->last_cycles = elapsed_cycles;
}

void Diagnostics_SetSchedulerStats(Diagnostics *d,
                                   uint32_t miss_1ms,
                                   uint32_t miss_10ms,
                                   uint32_t miss_100ms,
                                   uint32_t sci_rx_overflow)
{
    if (d == ((Diagnostics *)0)) return;
    d->miss_1ms   = miss_1ms;
    d->miss_10ms  = miss_10ms;
    d->miss_100ms = miss_100ms;
    d->sci_rx_overflow = sci_rx_overflow;
}

void Diagnostics_SetSystemDiag(Diagnostics *d,
                                uint16_t system_state,
                                uint16_t fault_code,
                                uint32_t fault_tick)
{
    if (d == ((Diagnostics *)0)) return;
    d->system_state = system_state;
    d->fault_code   = fault_code;
    d->fault_tick   = fault_tick;
}

void Diagnostics_SetParamStats(Diagnostics *d,
                               uint32_t commit_count,
                               uint32_t reject_count,
                               uint16_t last_reject_reason)
{
    if (d == ((Diagnostics *)0)) return;
    d->param_commit_count      = commit_count;
    d->param_reject_count      = reject_count;
    d->param_last_reject_reason = last_reject_reason;
}

void Diagnostics_SetTelemetryStats(Diagnostics *d,
                                   uint32_t write_count,
                                   uint32_t overrun_count)
{
    if (d == ((Diagnostics *)0)) return;
    d->telem_write_count   = write_count;
    d->telem_overrun_count = (uint16_t)overrun_count;
}

void Diagnostics_SetPwmSnapshot(Diagnostics *d,
                                uint16_t period,
                                uint16_t counter)
{
    if (d == ((Diagnostics *)0)) return;
    d->pwm_period  = period;
    d->pwm_counter = counter;
}

void Diagnostics_WriteDiagFlags(Diagnostics *d, uint32_t flags)
{
    if (d == ((Diagnostics *)0)) return;
    d->diag_flags = flags;
}
