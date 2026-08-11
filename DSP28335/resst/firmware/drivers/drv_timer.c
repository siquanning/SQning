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

/* ---- Timer2: free-running 32-bit cycle counter (nominal 150 MHz) ---- */

void DrvTimer2_CycleInit(void)
{
    CpuTimer2Regs.TCR.bit.TSS = 1U;          /* stop */

    CpuTimer2Regs.TPR.all  = 0U;             /* /1 prescaler */
    CpuTimer2Regs.TPRH.all = 0U;
    CpuTimer2Regs.PRD.all  = 0xFFFFFFFFUL;   /* 32-bit free-running */

    CpuTimer2Regs.TCR.bit.TRB  = 1U;         /* reload PRD→TIM */
    CpuTimer2Regs.TCR.bit.TIE  = 0U;         /* no interrupt */
    CpuTimer2Regs.TCR.bit.SOFT = 1U;
    CpuTimer2Regs.TCR.bit.FREE = 1U;         /* keep counting during debug halt */
    CpuTimer2Regs.TCR.bit.TSS  = 0U;         /* start */
}

uint32_t DrvTimer2_CycleRead(void)
{
    return CpuTimer2Regs.TIM.all;
}
