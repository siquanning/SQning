#include "include/common.h"
#include "bsp/gpio.h"

void gpio_init(void)
{
    EALLOW;
    // Set GPIO64-68 as GPIO (not peripheral)
    GpioCtrlRegs.GPCMUX1.bit.GPIO64 = 0;
    GpioCtrlRegs.GPCMUX1.bit.GPIO65 = 0;
    GpioCtrlRegs.GPCMUX1.bit.GPIO66 = 0;
    GpioCtrlRegs.GPCMUX1.bit.GPIO67 = 0;
    GpioCtrlRegs.GPCMUX1.bit.GPIO68 = 0;

    // Output direction
    GpioCtrlRegs.GPCDIR.bit.GPIO64 = 1;
    GpioCtrlRegs.GPCDIR.bit.GPIO65 = 1;
    GpioCtrlRegs.GPCDIR.bit.GPIO66 = 1;
    GpioCtrlRegs.GPCDIR.bit.GPIO67 = 1;
    GpioCtrlRegs.GPCDIR.bit.GPIO68 = 1;

    // GPIO12 → TZ1 (Trip Zone 1) input — hardware fault shutdown
    // MUX=1 selects TZ1 function, active low with internal pull-up
    GpioCtrlRegs.GPAMUX1.bit.GPIO12 = 1;
    GpioCtrlRegs.GPADIR.bit.GPIO12  = 0;
    GpioCtrlRegs.GPAPUD.bit.GPIO12  = 0;
    EDIS;

    // Initial state: all LEDs off
    GpioDataRegs.GPCCLEAR.bit.GPIO64 = 1;
    GpioDataRegs.GPCCLEAR.bit.GPIO65 = 1;
    GpioDataRegs.GPCCLEAR.bit.GPIO66 = 1;
    GpioDataRegs.GPCCLEAR.bit.GPIO67 = 1;
    GpioDataRegs.GPCCLEAR.bit.GPIO68 = 1;
}
