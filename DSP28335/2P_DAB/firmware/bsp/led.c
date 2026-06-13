#include "include/common.h"
#include "bsp/led.h"

void led_all_on(void)
{
    GpioDataRegs.GPCSET.bit.GPIO64 = 1;
    GpioDataRegs.GPCSET.bit.GPIO65 = 1;
    GpioDataRegs.GPCSET.bit.GPIO66 = 1;
    GpioDataRegs.GPCSET.bit.GPIO67 = 1;
    GpioDataRegs.GPCSET.bit.GPIO68 = 1;
}

void led_all_off(void)
{
    GpioDataRegs.GPCCLEAR.bit.GPIO64 = 1;
    GpioDataRegs.GPCCLEAR.bit.GPIO65 = 1;
    GpioDataRegs.GPCCLEAR.bit.GPIO66 = 1;
    GpioDataRegs.GPCCLEAR.bit.GPIO67 = 1;
    GpioDataRegs.GPCCLEAR.bit.GPIO68 = 1;
}

void led1_toggle(void)
{
    GpioDataRegs.GPCTOGGLE.bit.GPIO64 = 1;
}

void led3_on(void)
{
    GpioDataRegs.GPCSET.bit.GPIO66 = 1;
}

void led3_off(void)
{
    GpioDataRegs.GPCCLEAR.bit.GPIO66 = 1;
}

void led5_on(void)
{
    GpioDataRegs.GPCSET.bit.GPIO68 = 1;
}

void led5_off(void)
{
    GpioDataRegs.GPCCLEAR.bit.GPIO68 = 1;
}

void led5_toggle(void)
{
    GpioDataRegs.GPCTOGGLE.bit.GPIO68 = 1;
}
