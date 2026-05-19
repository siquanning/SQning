/*
 * epwm_config.c — ePWM6: 1kHz, 50% duty, GPIO10 output
 *
 * TBCLK = SYSCLK / (HSPCLKDIV × CLKDIV) = 150MHz / (4 × 1) = 37.5MHz
 * TBPRD = TBCLK / (2 × f_pwm) − 1 = 37500000 / 2000 − 1 = 18749
 */

#include "epwm_config.h"

void Init_EPWM6_1kHz_50Percent(void)
{
    EALLOW;
    SysCtrlRegs.PCLKCR1.bit.EPWM6ENCLK = 1;
    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 0;
    EDIS;

    // GPIO10 → ePWM6A
    EALLOW;
    GpioCtrlRegs.GPAMUX1.bit.GPIO10 = 1;
    GpioCtrlRegs.GPADIR.bit.GPIO10  = 1;
    EDIS;

    // 时基：递增-递减模式
    EPwm6Regs.TBCTL.bit.CTRMODE   = TB_COUNT_UPDOWN;
    EPwm6Regs.TBCTL.bit.HSPCLKDIV = TB_DIV4;
    EPwm6Regs.TBCTL.bit.CLKDIV    = TB_DIV1;
    EPwm6Regs.TBPHS.half.TBPHS    = 0;
    EPwm6Regs.TBPRD               = EPWM6_TBPRD;

    // 比较值
    EPwm6Regs.CMPA.half.CMPA      = EPWM6_CMPA_DEFAULT;

    // 动作限定
    EPwm6Regs.AQCTLA.bit.CAU = AQ_CLEAR;
    EPwm6Regs.AQCTLA.bit.CAD = AQ_SET;

    // 死区禁用
    EPwm6Regs.DBCTL.all = 0;

    EALLOW;
    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 1;
    EDIS;
}
