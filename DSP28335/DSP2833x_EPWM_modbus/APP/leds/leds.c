/*
 * leds.c — LED GPIO initialization (GPIO64-68, active-low)
 */

#include "leds.h"

void LED_Init(void)
{
    EALLOW;
    SysCtrlRegs.PCLKCR3.bit.GPIOINENCLK = 1;

    // LED1  GPIO68
    GpioCtrlRegs.GPCMUX1.bit.GPIO68 = 0;
    GpioCtrlRegs.GPCDIR.bit.GPIO68  = 1;
    GpioCtrlRegs.GPCPUD.bit.GPIO68  = 0;

    // LED2  GPIO67
    GpioCtrlRegs.GPCMUX1.bit.GPIO67 = 0;
    GpioCtrlRegs.GPCDIR.bit.GPIO67  = 1;
    GpioCtrlRegs.GPCPUD.bit.GPIO67  = 0;

    // LED3  GPIO66
    GpioCtrlRegs.GPCMUX1.bit.GPIO66 = 0;
    GpioCtrlRegs.GPCDIR.bit.GPIO66  = 1;
    GpioCtrlRegs.GPCPUD.bit.GPIO66  = 0;

    // LED4  GPIO65
    GpioCtrlRegs.GPCMUX1.bit.GPIO65 = 0;
    GpioCtrlRegs.GPCDIR.bit.GPIO65  = 1;
    GpioCtrlRegs.GPCPUD.bit.GPIO65  = 0;

    // LED5  GPIO64
    GpioCtrlRegs.GPCMUX1.bit.GPIO64 = 0;
    GpioCtrlRegs.GPCDIR.bit.GPIO64  = 1;
    GpioCtrlRegs.GPCPUD.bit.GPIO64  = 0;

    // GPIO10/11 被 ePWM6 占用，不再操作

    // 全部熄灭（高电平）
    GpioDataRegs.GPCSET.bit.GPIO68 = 1;
    GpioDataRegs.GPCSET.bit.GPIO67 = 1;
    GpioDataRegs.GPCSET.bit.GPIO66 = 1;
    GpioDataRegs.GPCSET.bit.GPIO65 = 1;
    GpioDataRegs.GPCSET.bit.GPIO64 = 1;

    EDIS;
}
