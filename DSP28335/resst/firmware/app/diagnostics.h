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
    WcetSlot adc_isr;
    WcetSlot main_loop;
    uint32_t miss_1ms;
    uint32_t miss_10ms;
    uint32_t miss_100ms;
    uint32_t sci_rx_overflow;
    uint32_t sci_rx_total;
    WcetSlot fast_isr;
    uint32_t fast_isr_count;
    uint32_t adc_isr_count;
    uint16_t adc_raw[2];
    uint16_t trip_flags;
    uint16_t pwm_period;
    uint16_t pwm_counter;

    /* Step 3: control / state / fault / param / telemetry */
    uint16_t system_state;
    uint16_t fault_code;
    uint32_t fault_tick;
    uint32_t param_commit_count;
    uint32_t param_reject_count;
    uint16_t param_last_reject_reason;
    uint16_t telem_overrun_count;
    uint32_t telem_write_count;
    uint32_t diag_flags;        /* DIAG_FLAG_* bitmap */
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

/*
 * Batch-setters for the 100ms diagnostic snapshot.
 * Each function updates a related group of fields atomically from
 * the App layer's perspective — App_Service100ms calls these instead
 * of writing Diagnostics fields directly.
 *
 * All setters are non-blocking, no dynamic memory, no formatting.
 */

/* Scheduler miss counters + SCI RX overflow */
void Diagnostics_SetSchedulerStats(Diagnostics *d,
                                   uint32_t miss_1ms,
                                   uint32_t miss_10ms,
                                   uint32_t miss_100ms,
                                   uint32_t sci_rx_overflow);

/* System state, fault code, and fault tick from StateMachine */
void Diagnostics_SetSystemDiag(Diagnostics *d,
                                uint16_t system_state,
                                uint16_t fault_code,
                                uint32_t fault_tick);

/* Parameter commit/reject statistics from ParamManager */
void Diagnostics_SetParamStats(Diagnostics *d,
                               uint32_t commit_count,
                               uint32_t reject_count,
                               uint16_t last_reject_reason);

/* Telemetry write count and overrun count */
void Diagnostics_SetTelemetryStats(Diagnostics *d,
                                   uint32_t write_count,
                                   uint32_t overrun_count);

/* PWM period and counter snapshot */
void Diagnostics_SetPwmSnapshot(Diagnostics *d,
                                uint16_t period,
                                uint16_t counter);

/* Update diag_flags bitmap (LOGICAL_RUN, etc.) */
void Diagnostics_WriteDiagFlags(Diagnostics *d, uint32_t flags);

#endif
