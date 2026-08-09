#include "DSP2833x_Device.h"
#include "DSP2833x_GlobalPrototypes.h"
#include "firmware/drivers/drv_timer.h"

/*
 * 100 us system tick counter.
 * uint32_t on C28x (16-bit machine) is non-atomic — see Timebase_Now().
 */
static volatile uint32_t g_sysTick;

void DrvTimer0_Init(uint32_t sysclk_mhz, uint32_t period_us)
{
    g_sysTick = 0UL;

    InitCpuTimers();
    ConfigCpuTimer(&CpuTimer0, (float)sysclk_mhz, (float)period_us);
}

void DrvTimer0_Start(void)
{
    CpuTimer0Regs.TCR.bit.TSS = 0;
}

void DrvTimer0_OnInterrupt(void)
{
    g_sysTick++;

    /* Clear Timer0 interrupt flag */
    CpuTimer0Regs.TCR.bit.TIF = 1;
}

/*
 * Safe 32-bit read on a 16-bit C28x: double-read until two consecutive
 * reads match, guaranteeing no ISR interleaved between halves.
 */
uint32_t Timebase_Now(void)
{
    uint32_t first, second;
    do
    {
        first  = g_sysTick;
        second = g_sysTick;
    } while (first != second);
    return first;
}
