/*
 * gpio_config.c — SCI-A GPIO 初始化（GPIO35/36，与 DSP2833x_EPWM_modbus 一致）
 */

#include "DSP2833x_Device.h"
#include "DSP2833x_Examples.h"
#include "gpio_config.h"

void Init_Scia_Gpio(void)
{
    EALLOW;

    // GPIO36 → SCIRXDA（输入，异步，内部上拉）
    GpioCtrlRegs.GPBMUX1.bit.GPIO36  = 1;
    GpioCtrlRegs.GPBDIR.bit.GPIO36   = 0;
    GpioCtrlRegs.GPBQSEL1.bit.GPIO36 = 3;
    GpioCtrlRegs.GPBPUD.bit.GPIO36   = 0;

    // GPIO35 → SCITXDA（输出，异步）
    GpioCtrlRegs.GPBMUX1.bit.GPIO35  = 1;
    GpioCtrlRegs.GPBDIR.bit.GPIO35   = 1;
    GpioCtrlRegs.GPBQSEL1.bit.GPIO35 = 3;
    GpioCtrlRegs.GPBPUD.bit.GPIO35   = 0;

    EDIS;
}
