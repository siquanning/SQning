#include "DSP2833x_Device.h"
#include "firmware/drivers/drv_sysctrl.h"

/*
 * PLL lock timeout: F28335 PLL typically locks within 1-2 ms.
 * At 150 MHz SYSCLKOUT, 200 000 loop iterations ≈ 13 ms worst case.
 * If PLLLOCKS is still not set after the timeout, the crystal or PLL
 * hardware is likely faulty — the caller MUST NOT continue with an
 * unknown system clock and must enter a safe stopped state.
 */
#define DRV_PLL_LOCK_TIMEOUT_LOOPS  200000U

bool DrvSysCtrl_Init(const SysClockConfig *config)
{
    uint32_t timeout;

    if (config == ((const SysClockConfig *)0))
    {
        return false;
    }

    /* ---- 1. Disable watchdog ---- */
    EALLOW;
    SysCtrlRegs.WDCR = 0x0068U;
    EDIS;

    /* ---- 2. PLL: set PLLCR, wait for lock, then set DIVSEL ---- */
    if (SysCtrlRegs.PLLSTS.bit.DIVSEL != 0U)
    {
        EALLOW;
        SysCtrlRegs.PLLSTS.bit.DIVSEL = 0U;
        EDIS;
    }

    EALLOW;
    SysCtrlRegs.PLLSTS.bit.MCLKOFF = 1U;
    SysCtrlRegs.PLLCR.bit.DIV      = config->pll_div;
    EDIS;

    /* Finite-time PLL lock poll — must not loop forever.
     * Strategy: if the PLL fails to lock within the timeout window,
     * return false.  The caller (main / BSP) is responsible for
     * entering a safe stopped state — no SCI/SPI/Timer init, as the
     * system clock is unreliable. */
    timeout = 0UL;
    while (SysCtrlRegs.PLLSTS.bit.PLLLOCKS != 1U)
    {
        if (++timeout >= DRV_PLL_LOCK_TIMEOUT_LOOPS)
        {
            return false;
        }
    }

    EALLOW;
    SysCtrlRegs.PLLSTS.bit.MCLKOFF = 0U;
    SysCtrlRegs.PLLSTS.bit.DIVSEL  = config->divsel;
    EDIS;

    /* ---- 3. Peripheral clock prescalers ---- */
    EALLOW;
    SysCtrlRegs.HISPCP.all = config->hispcp;
    SysCtrlRegs.LOSPCP.all = config->lospcp_div;
    EDIS;

    /* ---- 4. Peripheral clock enables — only what this bridge uses ---- */
    EALLOW;
    SysCtrlRegs.PCLKCR0.all = 0x0000U;
    SysCtrlRegs.PCLKCR1.all = 0x0000U;
    SysCtrlRegs.PCLKCR3.all = 0x0000U;
    SysCtrlRegs.PCLKCR0.bit.SCIAENCLK      = 1U;   /* SCI-A       */
    SysCtrlRegs.PCLKCR0.bit.SPIAENCLK      = 1U;   /* SPI-A       */
    SysCtrlRegs.PCLKCR3.bit.CPUTIMER0ENCLK = 1U;   /* CPU Timer0  */
    SysCtrlRegs.PCLKCR3.bit.GPIOINENCLK    = 1U;   /* GPIO input  */
    EDIS;

    return true;
}

/*
 * Flash wait-state/pipeline configuration for 150 MHz SYSCLKOUT.
 * CAUTION: This function MUST execute from RAM. The linker places it
 * in the "ramfuncs" section (LOAD in Flash, RUN in RAML03). Calling this
 * from Flash before wait-states are configured produces undefined behavior.
 *
 * Wait-state values per TI datasheet SPRS439N for 150 MHz:
 *   PAGEWAIT = 5, RANDWAIT = 5, OTPWAIT = 8
 */
#pragma CODE_SECTION(DrvFlash_Init, "ramfuncs")
void DrvFlash_Init(void)
{
    EALLOW;

    /* Enable Flash pipeline mode */
    FlashRegs.FOPT.bit.ENPIPE = 1;

    /* Wait states for 150 MHz SYSCLKOUT */
    FlashRegs.FBANKWAIT.bit.PAGEWAIT = 5;
    FlashRegs.FBANKWAIT.bit.RANDWAIT = 5;
    FlashRegs.FOTPWAIT.bit.OTPWAIT   = 8;

    /* Standby-to-active and sleep-to-standby delays (TI default) */
    FlashRegs.FSTDBYWAIT.bit.STDBYWAIT   = 0x01FF;
    FlashRegs.FACTIVEWAIT.bit.ACTIVEWAIT = 0x01FF;

    EDIS;

    /* Pipeline flush: ensure the last register write completes */
    asm(" RPT #7 || NOP");
}
