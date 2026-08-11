#include "DSP2833x_Device.h"
#include "firmware/drivers/drv_gpio.h"

/*
 * Narrow LED-only GPIO interface.
 * GPIO67/68 are on Port C — MUX1 covers GPIO64–79.
 * Active-low: CLEAR turns LED on, SET turns LED off.
 */

void DrvGpio_InitOutput(uint16_t pin, uint16_t initial_high)
{
    EALLOW;

    if (pin == 67U || pin == 68U)
    {
        GpioCtrlRegs.GPCMUX1.bit.GPIO67 = 0;
        GpioCtrlRegs.GPCMUX1.bit.GPIO68 = 0;

        GpioCtrlRegs.GPCDIR.bit.GPIO67 = 1;
        GpioCtrlRegs.GPCDIR.bit.GPIO68 = 1;

        if (initial_high)
        {
            GpioDataRegs.GPCSET.bit.GPIO67 = 1;
            GpioDataRegs.GPCSET.bit.GPIO68 = 1;
        }
        else
        {
            GpioDataRegs.GPCCLEAR.bit.GPIO67 = 1;
            GpioDataRegs.GPCCLEAR.bit.GPIO68 = 1;
        }
    }

    EDIS;
}

void DrvGpio_Set(uint16_t pin)
{
    if (pin == 67U)       GpioDataRegs.GPCSET.bit.GPIO67 = 1;
    else if (pin == 68U)  GpioDataRegs.GPCSET.bit.GPIO68 = 1;
}

void DrvGpio_Clear(uint16_t pin)
{
    if (pin == 67U)       GpioDataRegs.GPCCLEAR.bit.GPIO67 = 1;
    else if (pin == 68U)  GpioDataRegs.GPCCLEAR.bit.GPIO68 = 1;
}
