/*
 * app_modbus.c — ePWM6 ↔ Modbus register bridge
 */

#include "app_modbus.h"

static uint16_t CalcFreqFromTBPRD(uint16_t tbprd)
{
    if (tbprd == 0) return TBPRD_INVALID;
    return (uint16_t)(TBCLK_FREQ / (2UL * (tbprd + 1)));
}

static uint16_t FreqToTBPRD(uint16_t freq)
{
    if (freq == 0) return TBPRD_INVALID;
    uint32_t tmp = TBCLK_FREQ / (2UL * freq);
    if (tmp > 65535) return TBPRD_INVALID;
    return (tmp > 0) ? (uint16_t)(tmp - 1) : 0;
}

void MB_InitRegs(void)
{
    uint16_t tbprd = EPwm6Regs.TBPRD;
    uint16_t freq  = CalcFreqFromTBPRD(tbprd);
    HoldingRegs[0] = freq;

    uint16_t duty_tenth = 0;
    if (tbprd > 0)
        duty_tenth = (uint32_t)(EPwm6Regs.CMPA.half.CMPA) * 1000UL / (tbprd + 1);
    HoldingRegs[1] = duty_tenth;
}

bool MB_ApplyRegChanges(void)
{
    uint16_t req_freq       = HoldingRegs[0];
    uint16_t req_duty_tenth = HoldingRegs[1];

    if (req_freq < PWM_FREQ_MIN) req_freq = PWM_FREQ_MIN;
    if (req_freq > PWM_FREQ_MAX) req_freq = PWM_FREQ_MAX;

    uint16_t tbprd = FreqToTBPRD(req_freq);
    if (tbprd == TBPRD_INVALID || tbprd < PWM_TBPRD_MIN)
        return false;

    // 保持当前占空比重新计算 CMPA
    uint16_t cmp = (uint32_t)(req_duty_tenth) * (tbprd + 1) / 1000;
    if (cmp > tbprd) cmp = tbprd + 1;

    EALLOW;
    EPwm6Regs.TBCTL.bit.PRDLD      = 1;   // 影子寄存器使能
    EPwm6Regs.CMPCTL.bit.LOADAMODE = 0;   // CTR=0 时装载
    EPwm6Regs.TBPRD                = tbprd;
    EPwm6Regs.CMPA.half.CMPA       = cmp;
    EDIS;

    // 写回实际值
    HoldingRegs[0] = CalcFreqFromTBPRD(tbprd);
    HoldingRegs[1] = (uint32_t)cmp * 1000 / (tbprd + 1);
    return true;
}

void MB_ReadInputRegs(void)
{
    uint16_t tbprd = EPwm6Regs.TBPRD;
    uint16_t cmp   = EPwm6Regs.CMPA.half.CMPA;

    InputRegs[0] = CalcFreqFromTBPRD(tbprd);
    InputRegs[1] = (tbprd > 0) ? (uint32_t)cmp * 1000UL / (tbprd + 1) : 0;
}
