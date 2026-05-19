/*
 * epwm_config.c
 *
 *  Created on: 2026年5月12日
 *      Author: 32485
 */
#include "epwm_config.h"

void Init_EPWM6_1kHz_50Percent(void)
{
    EALLOW;
    // 1. 打开 ePWM6 模块时钟
    SysCtrlRegs.PCLKCR1.bit.EPWM6ENCLK = 1;
    // 2. 暂时停止时基时钟同步，方便配置
    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 0;
    EDIS;

    // 配置 GPIO10 为 ePWM6A
    EALLOW;
    GpioCtrlRegs.GPAMUX1.bit.GPIO10 = 1;
    GpioCtrlRegs.GPADIR.bit.GPIO10 = 1;
    EDIS;

    // 时基
    EPwm6Regs.TBCTL.bit.CTRMODE = TB_COUNT_UPDOWN;
    EPwm6Regs.TBCTL.bit.HSPCLKDIV = TB_DIV4;
    EPwm6Regs.TBCTL.bit.CLKDIV   = TB_DIV1;
    EPwm6Regs.TBPHS.half.TBPHS = 0;
    EPwm6Regs.TBPRD = 18749;

    // 比较器
    EPwm6Regs.CMPA.half.CMPA = 9375;

    // 动作限定
    EPwm6Regs.AQCTLA.bit.CAU = AQ_CLEAR;
    EPwm6Regs.AQCTLA.bit.CAD = AQ_SET;

    // 禁用死区
    EPwm6Regs.DBCTL.all = 0;

    // 恢复时基时钟同步，启动 PWM
    EALLOW;
    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 1;
    EDIS;
}

