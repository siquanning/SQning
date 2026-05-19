/*
 * gpio_config.c
 *
 *  Created on: 2026年5月9日
 *      Author: 32485
 */

#include "DSP2833x_Device.h"
#include "DSP2833x_Examples.h"
#include "gpio_config.h"

void Init_Scia_Gpio(void)
{
    EALLOW;  // 允许修改保护寄存器

    // --- GPIO36 配置为 SCIRXDA (接收) ---
    GpioCtrlRegs.GPBMUX1.bit.GPIO36 = 1;   // 设置为SCI功能 (01b)
    GpioCtrlRegs.GPBDIR.bit.GPIO36 = 0;    // 设置为输入
    GpioCtrlRegs.GPBQSEL1.bit.GPIO36 = 3;  // 异步输入模式 (11b)
    GpioCtrlRegs.GPBPUD.bit.GPIO36 = 0;    // 使能内部上拉

    // --- GPIO35 配置为 SCITXDA (发送) ---
    GpioCtrlRegs.GPBMUX1.bit.GPIO35 = 1;   // 设置为SCI功能 (01b)
    GpioCtrlRegs.GPBDIR.bit.GPIO35 = 1;    // 设置为输出
    GpioCtrlRegs.GPBQSEL1.bit.GPIO35 = 3;  // 异步输入模式 (配置输出时也推荐)
    GpioCtrlRegs.GPBPUD.bit.GPIO35 = 0;    // 使能内部上拉

    EDIS;    // 禁止修改保护寄存器
}



