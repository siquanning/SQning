/*
 * gpio_config.c — SCI-A GPIO pin initialization
 */

#include "DSP2833x_Device.h"
#include "DSP2833x_Examples.h"
#include "gpio_config.h"

void Init_Scia_Gpio(void)
{
    EALLOW;

    // GPIO36 → SCIRXDA (input, async, pull-up)
    GpioCtrlRegs.GPBMUX1.bit.GPIO36  = 1;
    GpioCtrlRegs.GPBDIR.bit.GPIO36   = 0;
    GpioCtrlRegs.GPBQSEL1.bit.GPIO36 = 3;
    GpioCtrlRegs.GPBPUD.bit.GPIO36   = 0;

    // GPIO35 → SCITXDA (output, async)
    GpioCtrlRegs.GPBMUX1.bit.GPIO35  = 1;
    GpioCtrlRegs.GPBDIR.bit.GPIO35   = 1;
    GpioCtrlRegs.GPBQSEL1.bit.GPIO35 = 3;
    GpioCtrlRegs.GPBPUD.bit.GPIO35   = 0;

    EDIS;
}
