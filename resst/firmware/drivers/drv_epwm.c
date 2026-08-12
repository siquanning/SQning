#include "DSP2833x_Device.h"
#include "DSP2833x_EPwm_defines.h"
#include "firmware/drivers/drv_epwm.h"
#include "firmware/bsp/board_config.h"

static volatile struct EPWM_REGS *EpwmReg(uint32_t module)
{
    if (module == 1U) return &EPwm1Regs;
    if (module == 2U) return &EPwm2Regs;
    if (module == 3U) return &EPwm3Regs;
    if (module == 4U) return &EPwm4Regs;
    if (module == 5U) return &EPwm5Regs;
    if (module == 6U) return &EPwm6Regs;
    return ((volatile struct EPWM_REGS *)0);
}

static Uint16 ClampU16(uint32_t val, Uint16 lo, Uint16 hi)
{
    if (val < (uint32_t)lo) return lo;
    if (val > (uint32_t)hi) return hi;
    return (Uint16)val;
}

int32_t DrvEpwm_Init(uint32_t module, const DrvEpwmConfig *cfg)
{
    volatile struct EPWM_REGS *epwm;
    Uint16 tbprd;

    if (cfg == ((const DrvEpwmConfig *)0))
        return -1;
    epwm = EpwmReg(module);
    if (epwm == ((volatile struct EPWM_REGS *)0))
        return -2;

    tbprd = (Uint16)(cfg->tbclk_hz / (2UL * cfg->pwm_freq_hz));
    if (tbprd < 1U)
        return -3;

    EALLOW;

    /* Enable ePWM peripheral clock for this module */
    if (module == 1U) SysCtrlRegs.PCLKCR1.bit.EPWM1ENCLK = 1U;
    else if (module == 2U) SysCtrlRegs.PCLKCR1.bit.EPWM2ENCLK = 1U;
    else if (module == 3U) SysCtrlRegs.PCLKCR1.bit.EPWM3ENCLK = 1U;
    else if (module == 4U) SysCtrlRegs.PCLKCR1.bit.EPWM4ENCLK = 1U;
    else if (module == 5U) SysCtrlRegs.PCLKCR1.bit.EPWM5ENCLK = 1U;
    else if (module == 6U) SysCtrlRegs.PCLKCR1.bit.EPWM6ENCLK = 1U;

    /* ---- Time-base ---- */
    epwm->TBPRD = tbprd;
    epwm->TBCTR = 0U;
    epwm->TBPHS.half.TBPHS = 0U;
    epwm->TBCTL.all = 0U;
    epwm->TBCTL.bit.CTRMODE    = cfg->count_mode;
    epwm->TBCTL.bit.PHSEN      = TB_DISABLE;
    epwm->TBCTL.bit.PRDLD      = TB_SHADOW;
    epwm->TBCTL.bit.SYNCOSEL   = TB_CTR_ZERO;
    epwm->TBCTL.bit.HSPCLKDIV  = TB_DIV1;
    epwm->TBCTL.bit.CLKDIV     = TB_DIV1;
    epwm->TBCTL.bit.FREE_SOFT  = 0U;

    /* ---- Compare: safe minimum values ---- */
    epwm->CMPA.half.CMPA = 0U;
    epwm->CMPB           = 0U;

    /* ---- Compare control: shadow mode, load at CTR=ZERO ---- */
    epwm->CMPCTL.all = 0U;
    epwm->CMPCTL.bit.SHDWAMODE = CC_SHADOW;
    epwm->CMPCTL.bit.SHDWBMODE = CC_SHADOW;
    epwm->CMPCTL.bit.LOADAMODE = CC_CTR_ZERO;
    epwm->CMPCTL.bit.LOADBMODE = CC_CTR_ZERO;

    /* ---- Action qualifier: CMPA→EPWMxA, same CMPA→EPWMxB (complementary) ---- */
    epwm->AQCTLA.all = 0U;
    epwm->AQCTLB.all = 0U;
    epwm->AQCTLA.bit.CAU = AQ_CLEAR;
    epwm->AQCTLA.bit.CAD = AQ_SET;
    epwm->AQCTLB.bit.CAU = AQ_SET;
    epwm->AQCTLB.bit.CAD = AQ_CLEAR;

    /* ---- Software force: HOLD BOTH OUTPUTS LOW (primary safety) ---- */
    epwm->AQSFRC.all  = 0U;
    epwm->AQCSFRC.all = 0U;
    epwm->AQCSFRC.bit.CSFA = 1U;   /* Force EPWMxA LOW continuously */
    epwm->AQCSFRC.bit.CSFB = 1U;   /* Force EPWMxB LOW continuously */

    /* ---- Dead-band: DSP generates complementary outputs with dead-time ---- */
    epwm->DBCTL.all = 0U;
    if (cfg->db_red > 0U || cfg->db_fed > 0U)
    {
        /*
         * IN_MODE=DBA_ALL:  EPWMxA is the source for both edges.
         * POLSEL=DB_ACTV_HIC:  A = active-high,  B = active-low complementary.
         * OUT_MODE=DB_FULL_ENABLE:  RED delays rising edge, FED delays falling edge.
         *
         * EPWMxA (active-high) and EPWMxB (active-low complementary) now share
         * the same source waveform.  With dead-band enabled, AQCTLB and CMPB are
         * no longer used for B output generation — B is derived from A via DB.
         */
        epwm->DBCTL.bit.IN_MODE  = DBA_ALL;
        epwm->DBCTL.bit.POLSEL   = DB_ACTV_HIC;
        epwm->DBCTL.bit.OUT_MODE = DB_FULL_ENABLE;
        epwm->DBRED = (Uint16)(cfg->db_red & 0x3FFU);
        epwm->DBFED = (Uint16)(cfg->db_fed & 0x3FFU);
    }

    /* ---- Trip Zone: select sources, force-LO action, enable OST interrupt ---- */
    epwm->TZSEL.all = cfg->tz_sources;
    epwm->TZCTL.all = 0U;
    epwm->TZCTL.bit.TZA = TZ_FORCE_LO;
    epwm->TZCTL.bit.TZB = TZ_FORCE_LO;

    epwm->TZEINT.all = 0U;
    epwm->TZEINT.bit.OST = 1U;

    epwm->TZCLR.all = 0xFFFFU;
    epwm->TZFRC.all = 0U;

    /* ---- Event trigger: SOCA on CTR=ZERO (drives ADC), disabled until BSP gate ---- */
    epwm->ETSEL.all = 0U;
    epwm->ETSEL.bit.SOCASEL = ET_CTR_ZERO;
    epwm->ETSEL.bit.SOCAEN  = 0U;
    epwm->ETSEL.bit.INTEN   = 0U;

    epwm->ETPS.all = 0U;
    epwm->ETPS.bit.SOCAPRD = ET_1ST;

    epwm->ETCLR.all = 0xFFFFU;

    /* ---- Chopper: disabled ---- */
    epwm->PCCTL.all = 0U;

    EDIS;

    return 0;
}

void DrvEpwm_HaltTimebase(void)
{
    EALLOW;
    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 0U;
    EDIS;
}

void DrvEpwm_ConfigGpio(uint32_t module)
{
    static uint16_t tz_configured = 0U;

#if BOARD_PWM_ADC_HW_CONFIRMED == 1U
    EALLOW;

    /* ePWMxA and ePWMxB pin mux per module (F28335 device-specific) */
    if (module == 1U)
    {
        GpioCtrlRegs.GPAMUX1.bit.GPIO0 = 1U;   /* EPWM1A */
        GpioCtrlRegs.GPAMUX1.bit.GPIO1 = 1U;   /* EPWM1B */
    }
    else if (module == 2U)
    {
        GpioCtrlRegs.GPAMUX1.bit.GPIO2 = 1U;   /* EPWM2A */
        GpioCtrlRegs.GPAMUX1.bit.GPIO3 = 1U;   /* EPWM2B */
    }
    else if (module == 3U)
    {
        GpioCtrlRegs.GPAMUX1.bit.GPIO4 = 1U;   /* EPWM3A */
        GpioCtrlRegs.GPAMUX1.bit.GPIO5 = 1U;   /* EPWM3B */
    }
    else if (module == 4U)
    {
        GpioCtrlRegs.GPAMUX1.bit.GPIO6 = 1U;   /* EPWM4A */
        GpioCtrlRegs.GPAMUX1.bit.GPIO7 = 1U;   /* EPWM4B */
    }
    else if (module == 5U)
    {
        GpioCtrlRegs.GPAMUX1.bit.GPIO8 = 1U;   /* EPWM5A */
        GpioCtrlRegs.GPAMUX1.bit.GPIO9 = 1U;   /* EPWM5B */
    }
    else if (module == 6U)
    {
        GpioCtrlRegs.GPAMUX1.bit.GPIO10 = 1U;  /* EPWM6A */
        GpioCtrlRegs.GPAMUX1.bit.GPIO11 = 1U;  /* EPWM6B */
    }

    /* TZ1/TZ2 GPIOs: shared across all modules, configure once */
    if (tz_configured == 0U)
    {
        GpioCtrlRegs.GPAMUX1.bit.GPIO12 = 1U;  /* TZ1 */
        GpioCtrlRegs.GPAMUX1.bit.GPIO13 = 1U;  /* TZ2 */
        tz_configured = 1U;
    }

    EDIS;
#else
    (void)module;
    (void)tz_configured;
#endif
}

void DrvEpwm_SetCompareA(uint32_t module, uint16_t value,
                         uint16_t max_duty_permill)
{
    volatile struct EPWM_REGS *epwm = EpwmReg(module);
    Uint16 tbprd;
    Uint16 hi;
    if (epwm == ((volatile struct EPWM_REGS *)0))
        return;

    tbprd = epwm->TBPRD;
    hi = (Uint16)(((uint32_t)tbprd * (uint32_t)max_duty_permill) / 1000U);
    epwm->CMPA.half.CMPA = ClampU16((uint32_t)value, 0U, hi);
}

void DrvEpwm_SetCompareB(uint32_t module, uint16_t value,
                         uint16_t max_duty_permill)
{
    volatile struct EPWM_REGS *epwm = EpwmReg(module);
    Uint16 tbprd;
    Uint16 hi;
    if (epwm == ((volatile struct EPWM_REGS *)0))
        return;

    tbprd = epwm->TBPRD;
    hi = (Uint16)(((uint32_t)tbprd * (uint32_t)max_duty_permill) / 1000U);
    epwm->CMPB = ClampU16((uint32_t)value, 0U, hi);
}

void DrvEpwm_EnablePeriodInt(uint32_t module)
{
    volatile struct EPWM_REGS *epwm = EpwmReg(module);
    if (epwm == ((volatile struct EPWM_REGS *)0))
        return;

    EALLOW;
    epwm->ETSEL.bit.INTSEL = ET_CTR_ZERO;   /* Interrupt at CTR=ZERO */
    epwm->ETSEL.bit.INTEN  = 1U;            /* Enable */
    epwm->ETPS.bit.INTPRD  = ET_1ST;        /* Every event */
    EDIS;
}

void DrvEpwm_ClearIntFlag(uint32_t module)
{
    volatile struct EPWM_REGS *epwm = EpwmReg(module);
    if (epwm == ((volatile struct EPWM_REGS *)0))
        return;

    EALLOW;
    epwm->ETCLR.bit.INT = 1U;
    EDIS;
}

void DrvEpwm_EnableAdcSocA(uint32_t module)
{
    volatile struct EPWM_REGS *epwm = EpwmReg(module);
    if (epwm == ((volatile struct EPWM_REGS *)0))
        return;

    EALLOW;
    epwm->ETSEL.bit.SOCAEN = 1U;
    EDIS;
}

void DrvEpwm_ForceTrip(uint32_t module)
{
    volatile struct EPWM_REGS *epwm = EpwmReg(module);
    if (epwm == ((volatile struct EPWM_REGS *)0))
        return;

    /* Force OST trip → TZCTL action (TZ_FORCE_LO) blocks both outputs.
     * Does NOT touch AQCSFRC — modulation state is preserved. */
    EALLOW;
    epwm->TZFRC.bit.OST = 1U;
    EDIS;
}

void DrvEpwm_SetLegForceHighPair(uint32_t module,
                                  uint16_t force_a,
                                  uint16_t force_b)
{
    volatile struct EPWM_REGS *epwm = EpwmReg(module);
    uint16_t value;

    if (epwm == ((volatile struct EPWM_REGS *)0))
        return;

    /*
     * CSFA bits[1:0]: 00=disabled, 10=force HIGH (continuous)
     * CSFB bits[3:2]: 00=disabled, 10=force HIGH (continuous)
     * AQSFRC.RLDCSF=0 (CTR=ZERO) from DrvEpwm_Init — shadow loads at period boundary.
     */
    value = 0U;
    if (force_a != 0U) { value |= 0x0002U; }
    if (force_b != 0U) { value |= 0x0008U; }

    EALLOW;
    epwm->AQCSFRC.all = value;
    EDIS;
}

void DrvEpwm_SetHalfBridgeForceHigh(uint32_t module, uint16_t force_high)
{
    volatile struct EPWM_REGS *epwm = EpwmReg(module);

    if (epwm == ((volatile struct EPWM_REGS *)0))
        return;

    EALLOW;
    if (force_high != 0U)
    {
        /*
         * A = HIGH (CSFA = 10b)
         * B = LOW  (CSFB = 01b)
         */
        epwm->AQCSFRC.all = 0x0006U;
    }
    else
    {
        /* Release — AQCTLA/AQCTLB generate complementary PWM from CMPA */
        epwm->AQCSFRC.all = 0x0000U;
    }
    EDIS;
}

uint16_t DrvEpwm_GetTripStatus(uint32_t module)
{
    volatile struct EPWM_REGS *epwm = EpwmReg(module);
    if (epwm == ((volatile struct EPWM_REGS *)0))
        return 0U;
    return epwm->TZFLG.all;
}

void DrvEpwm_ClearCbcTrip(uint32_t module)
{
    volatile struct EPWM_REGS *epwm = EpwmReg(module);
    if (epwm == ((volatile struct EPWM_REGS *)0))
        return;

    EALLOW;
    epwm->TZCLR.bit.CBC = 1U;
    epwm->TZCLR.bit.INT = 1U;
    EDIS;
}

void DrvEpwm_ClearOstTrip(uint32_t module)
{
    volatile struct EPWM_REGS *epwm = EpwmReg(module);
    if (epwm == ((volatile struct EPWM_REGS *)0))
        return;

    EALLOW;
    epwm->TZCLR.bit.OST = 1U;
    epwm->TZCLR.bit.INT = 1U;
    epwm->TZEINT.bit.OST = 1U;   /* re-arm for next trip */
    EDIS;
}

void DrvEpwm_DisableOstInt(uint32_t module)
{
    volatile struct EPWM_REGS *epwm = EpwmReg(module);
    if (epwm == ((volatile struct EPWM_REGS *)0))
        return;

    EALLOW;
    epwm->TZEINT.bit.OST = 0U;
    epwm->TZCLR.bit.INT = 1U;
    EDIS;
}

uint16_t DrvEpwm_GetPeriod(uint32_t module)
{
    volatile struct EPWM_REGS *epwm = EpwmReg(module);
    if (epwm == ((volatile struct EPWM_REGS *)0))
        return 0U;
    return epwm->TBPRD;
}

uint16_t DrvEpwm_GetCounter(uint32_t module)
{
    volatile struct EPWM_REGS *epwm = EpwmReg(module);
    if (epwm == ((volatile struct EPWM_REGS *)0))
        return 0U;
    return epwm->TBCTR;
}
