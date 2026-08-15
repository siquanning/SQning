#include "DSP2833x_Device.h"
#include "firmware/drivers/drv_gpio.h"

/* ---- LED (Port C, active-low) ---- */

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

/* ---- UNI Polarity (Port A, GPIO27–29) ---- */

void DrvGpio_InitUniPolarity(void)
{
    EALLOW;

    /* GPIO27 → UNI_A_POS, GPIO28 → UNI_B_POS, GPIO29 → UNI_C_POS */
    GpioCtrlRegs.GPAMUX2.bit.GPIO27 = 0U;
    GpioCtrlRegs.GPAMUX2.bit.GPIO28 = 0U;
    GpioCtrlRegs.GPAMUX2.bit.GPIO29 = 0U;

    GpioCtrlRegs.GPADIR.bit.GPIO27 = 1U;
    GpioCtrlRegs.GPADIR.bit.GPIO28 = 1U;
    GpioCtrlRegs.GPADIR.bit.GPIO29 = 1U;

    /* Disable internal pull-ups on push-pull outputs */
    GpioCtrlRegs.GPAPUD.bit.GPIO27 = 0U;
    GpioCtrlRegs.GPAPUD.bit.GPIO28 = 0U;
    GpioCtrlRegs.GPAPUD.bit.GPIO29 = 0U;

    /* Default all LOW — safe state: no leg chopping, bridge blocked */
    GpioDataRegs.GPACLEAR.bit.GPIO27 = 1U;
    GpioDataRegs.GPACLEAR.bit.GPIO28 = 1U;
    GpioDataRegs.GPACLEAR.bit.GPIO29 = 1U;

    EDIS;
}

void DrvGpio_WriteUniPolarity(uint16_t uni_a, uint16_t uni_b, uint16_t uni_c)
{
    /*
     * Atomic GPA update inside a single EALLOW/EDIS.
     * Called from EPWM1 ISR at CTR=ZERO — same boundary as CMPA/AQCSFRC
     * shadow load.
     */
    EALLOW;

    if (uni_a != 0U)
        GpioDataRegs.GPASET.bit.GPIO27 = 1U;      /* GPIO27 = UNI_A_POS */
    else
        GpioDataRegs.GPACLEAR.bit.GPIO27 = 1U;

    if (uni_b != 0U)
        GpioDataRegs.GPASET.bit.GPIO28 = 1U;      /* GPIO28 = UNI_B_POS */
    else
        GpioDataRegs.GPACLEAR.bit.GPIO28 = 1U;

    if (uni_c != 0U)
        GpioDataRegs.GPASET.bit.GPIO29 = 1U;      /* GPIO29 = UNI_C_POS */
    else
        GpioDataRegs.GPACLEAR.bit.GPIO29 = 1U;

    EDIS;
}

/* ---- CPLD LED Heartbeat (Port A, GPIO26) ---- */

void DrvGpio_InitCpldLed(void)
{
    EALLOW;

    /* GPIO26 → CPLD GPIOK4 / PIN_111, push-pull output LOW */
    GpioCtrlRegs.GPAMUX2.bit.GPIO26 = 0U;
    GpioCtrlRegs.GPADIR.bit.GPIO26  = 1U;
    GpioCtrlRegs.GPAPUD.bit.GPIO26  = 0U;
    GpioDataRegs.GPACLEAR.bit.GPIO26 = 1U;

    EDIS;
}

void DrvGpio_ToggleCpldLed(void)
{
    uint32_t val = GpioDataRegs.GPADAT.all;

    if (val & (1UL << 26U))
        GpioDataRegs.GPACLEAR.bit.GPIO26 = 1U;
    else
        GpioDataRegs.GPASET.bit.GPIO26 = 1U;
}

/* ---- Run/Stop button + run state indicator (Port A, GPIO21/20) ---- */

void DrvGpio_InitRunButton(void)
{
    EALLOW;

    /* GPIO21 → 启停按钮输入 (EQEP1B mux off) */
    GpioCtrlRegs.GPAMUX2.bit.GPIO21 = 0U;
    GpioCtrlRegs.GPADIR.bit.GPIO21  = 0U;

    /* CPLD 推挽驱动: 松开=LOW，按下=HIGH；关闭 DSP 内部上拉 */
    GpioCtrlRegs.GPAPUD.bit.GPIO21  = 1U;

    EDIS;
}

uint16_t DrvGpio_ReadRunButton(void)
{
    return (GpioDataRegs.GPADAT.bit.GPIO21 != 0U) ? 1U : 0U;
}

void DrvGpio_InitRunState(void)
{
    EALLOW;

    /* GPIO20 → 运行状态 LED，高有效 (EQEP1A mux off) */
    GpioCtrlRegs.GPAMUX2.bit.GPIO20 = 0U;
    GpioCtrlRegs.GPADIR.bit.GPIO20  = 1U;

    /* Disable internal pull-up on push-pull output */
    GpioCtrlRegs.GPAPUD.bit.GPIO20  = 1U;

    /* Power-up safe: LOW = LED OFF = 未运行 */
    GpioDataRegs.GPACLEAR.bit.GPIO20 = 1U;

    EDIS;
}

void DrvGpio_WriteRunState(uint16_t level)
{
    if (level != 0U)
        GpioDataRegs.GPASET.bit.GPIO20 = 1U;    /* RUN: LED ON */
    else
        GpioDataRegs.GPACLEAR.bit.GPIO20 = 1U;  /* STOP: LED OFF */
}

/* ---- Grid input switch + precharge bypass (Port A, GPIO22/23) ---- */

void DrvGpio_InitGridSwitch(void)
{
    EALLOW;

    /* GPIO22：先预置LOW再切为输出，避免配置方向时误合S1/S2/S3。 */
    GpioDataRegs.GPACLEAR.bit.GPIO22 = 1U;
    GpioCtrlRegs.GPAMUX2.bit.GPIO22 = 0U;
    GpioCtrlRegs.GPADIR.bit.GPIO22  = 1U;
    GpioCtrlRegs.GPAPUD.bit.GPIO22  = 1U;

    EDIS;
}

void DrvGpio_WriteGridSwitch(uint16_t on)
{
    if (on != 0U)
        GpioDataRegs.GPASET.bit.GPIO22 = 1U;
    else
        GpioDataRegs.GPACLEAR.bit.GPIO22 = 1U;
}

void DrvGpio_InitPrechargeBypass(void)
{
    EALLOW;

    /* GPIO23：先预置LOW再切为输出，避免配置方向时误旁路预充电阻。 */
    GpioDataRegs.GPACLEAR.bit.GPIO23 = 1U;
    GpioCtrlRegs.GPAMUX2.bit.GPIO23 = 0U;
    GpioCtrlRegs.GPADIR.bit.GPIO23  = 1U;
    GpioCtrlRegs.GPAPUD.bit.GPIO23  = 1U;

    EDIS;
}

void DrvGpio_WritePrechargeBypass(uint16_t on)
{
    if (on != 0U)
        GpioDataRegs.GPASET.bit.GPIO23 = 1U;
    else
        GpioDataRegs.GPACLEAR.bit.GPIO23 = 1U;
}

/* ---- PWM_ENABLE / FAULT_GATE (Port A, GPIO30) ---- */

void DrvGpio_InitFaultGate(void)
{
    EALLOW;

    /* GPIO30 → CPLD G8: 0 = gates blocked */
    GpioCtrlRegs.GPAMUX2.bit.GPIO30 = 0U;
    GpioCtrlRegs.GPADIR.bit.GPIO30  = 1U;
    GpioCtrlRegs.GPAPUD.bit.GPIO30  = 0U;
    GpioDataRegs.GPACLEAR.bit.GPIO30 = 1U;

    EDIS;
}

void DrvGpio_WriteFaultGate(uint16_t level)
{
    /*
     * GPASET/GPACLEAR do not require EALLOW — safe from ISR context.
     * Called from PWM_BlockOutput/PWM_ReleaseOutput (background) and
     * System_EnterFault (possibly ISR).
     */
    if (level != 0U)
        GpioDataRegs.GPASET.bit.GPIO30 = 1U;    /* PWM_ENABLE = RUN */
    else
        GpioDataRegs.GPACLEAR.bit.GPIO30 = 1U;  /* FAULT_GATE = gates blocked */
}
