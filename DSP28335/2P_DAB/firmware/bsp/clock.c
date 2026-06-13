#include "include/common.h"
#include "bsp/clock.h"

void clock_init(void)
{
    EALLOW;
    // Disable watchdog
    SysCtrlRegs.WDCR = 0x0068;

    // Check for missing clock (limp mode)
    if (SysCtrlRegs.PLLSTS.bit.MCLKSTS != 0) {
        asm(" ESTOP0");
    }

    // DIVSEL must be 0 before PLLCR can be changed
    if (SysCtrlRegs.PLLSTS.bit.DIVSEL != 0) {
        SysCtrlRegs.PLLSTS.bit.DIVSEL = 0;
    }

    // Turn off missing clock detect before changing PLLCR
    SysCtrlRegs.PLLSTS.bit.MCLKOFF = 1;
    SysCtrlRegs.PLLCR.bit.DIV = 10;      // PLL x10 -> VCO = 300MHz
    EDIS;

    // Disable watchdog before PLL lock wait
    SysCtrlRegs.WDCR = 0x0068;

    // Wait for PLL to lock
    while (SysCtrlRegs.PLLSTS.bit.PLLLOCKS != 1) {}

    EALLOW;
    SysCtrlRegs.PLLSTS.bit.MCLKOFF = 0;

    // Switch to /2 -> SYSCLKOUT = 150MHz (F28335 max)
    SysCtrlRegs.PLLSTS.bit.DIVSEL = 2;

    // Enable GPIO input clock
    SysCtrlRegs.PCLKCR3.bit.GPIOINENCLK = 1;
    EDIS;
}
