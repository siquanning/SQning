#include "DSP2833x_Device.h"
#include "firmware/drivers/drv_adc.h"
#include "firmware/bsp/board_config.h"

/* Factory calibration routine programmed in TI-reserved OTP. */
#define DRV_ADC_CAL_FUNC              ((void (*)(void))0x00380080UL)
#define DRV_ADC_POWERUP_DELAY_US      5000UL

/* NOP loop provided by TI SRC/DSP2833x_usDelay.asm. */
extern void DSP28x_usDelay(Uint32 Count);

static void DrvAdc_DelayUs(uint32_t microseconds)
{
    uint32_t cycles = microseconds * (uint32_t)BOARD_SYSCLK_MHZ;
    uint32_t loop_count;

    /* DSP28x_usDelay consumes 9 cycles of overhead plus 5 per loop. */
    loop_count = (cycles > 9UL) ? ((cycles - 9UL) / 5UL) : 1UL;
    DSP28x_usDelay((Uint32)loop_count);
}

int32_t DrvAdc_Init(const DrvAdcConfig *cfg)
{
    if (cfg == ((const DrvAdcConfig *)0))
        return -1;
    if (cfg->num_channels == 0U || cfg->num_channels > 16U)
        return -1;

    /* Enable the peripheral clock before reset/calibration. */
    EALLOW;
    SysCtrlRegs.PCLKCR0.bit.ADCENCLK = 1U;
    EDIS;

    /* Reset first so calibration values are not subsequently discarded. */
    AdcRegs.ADCTRL1.bit.RESET = 1U;
    asm(" RPT #22 || NOP");

    /* Load device-specific ADCREFSEL/ADCOFFTRIM values from OTP. */
    EALLOW;
    DRV_ADC_CAL_FUNC();

    /* Select the internal reference before powering the analog circuits. */
    AdcRegs.ADCREFSEL.bit.REF_SEL = 0U;

    /* Power all ADC analog circuits, then observe the required 5 ms delay. */
    AdcRegs.ADCTRL3.bit.ADCBGRFDN = 0x3U;
    AdcRegs.ADCTRL3.bit.ADCPWDN   = 1U;
    EDIS;
    DrvAdc_DelayUs(DRV_ADC_POWERUP_DELAY_US);

    /* ADC clock divider */
    EALLOW;
    AdcRegs.ADCTRL3.bit.ADCCLKPS = cfg->adcclkps;
    EDIS;

    /* Acquisition window, CPS, cascaded sequencer */
    AdcRegs.ADCTRL1.bit.ACQ_PS   = cfg->acq_ps;
    AdcRegs.ADCTRL1.bit.CPS      = cfg->cps;
    AdcRegs.ADCTRL1.bit.SEQ_CASC = 1U;  /* Cascaded mode (SEQ1+SEQ2 = single 16-slot sequencer) */
    AdcRegs.ADCTRL1.bit.CONT_RUN = 0U;  /* Start-stop mode, not continuous */
    AdcRegs.ADCTRL1.bit.SEQ_OVRD = 0U;  /* Normal sequencer operation */

    /* Max conversions for SEQ1 */
    AdcRegs.ADCMAXCONV.all = 0U;
    AdcRegs.ADCMAXCONV.bit.MAX_CONV1 = (Uint16)(cfg->num_channels - 1U);

    /* Clear all channel-select registers, then program */
    AdcRegs.ADCCHSELSEQ1.all = 0U;
    AdcRegs.ADCCHSELSEQ2.all = 0U;
    AdcRegs.ADCCHSELSEQ3.all = 0U;
    AdcRegs.ADCCHSELSEQ4.all = 0U;

    if (cfg->num_channels > 0U)  AdcRegs.ADCCHSELSEQ1.bit.CONV00 = cfg->channels[0];
    if (cfg->num_channels > 1U)  AdcRegs.ADCCHSELSEQ1.bit.CONV01 = cfg->channels[1];
    if (cfg->num_channels > 2U)  AdcRegs.ADCCHSELSEQ1.bit.CONV02 = cfg->channels[2];
    if (cfg->num_channels > 3U)  AdcRegs.ADCCHSELSEQ1.bit.CONV03 = cfg->channels[3];
    if (cfg->num_channels > 4U)  AdcRegs.ADCCHSELSEQ2.bit.CONV04 = cfg->channels[4];
    if (cfg->num_channels > 5U)  AdcRegs.ADCCHSELSEQ2.bit.CONV05 = cfg->channels[5];
    if (cfg->num_channels > 6U)  AdcRegs.ADCCHSELSEQ2.bit.CONV06 = cfg->channels[6];
    if (cfg->num_channels > 7U)  AdcRegs.ADCCHSELSEQ2.bit.CONV07 = cfg->channels[7];
    if (cfg->num_channels > 8U)  AdcRegs.ADCCHSELSEQ3.bit.CONV08 = cfg->channels[8];
    if (cfg->num_channels > 9U)  AdcRegs.ADCCHSELSEQ3.bit.CONV09 = cfg->channels[9];
    if (cfg->num_channels > 10U) AdcRegs.ADCCHSELSEQ3.bit.CONV10 = cfg->channels[10];
    if (cfg->num_channels > 11U) AdcRegs.ADCCHSELSEQ3.bit.CONV11 = cfg->channels[11];
    if (cfg->num_channels > 12U) AdcRegs.ADCCHSELSEQ4.bit.CONV12 = cfg->channels[12];
    if (cfg->num_channels > 13U) AdcRegs.ADCCHSELSEQ4.bit.CONV13 = cfg->channels[13];
    if (cfg->num_channels > 14U) AdcRegs.ADCCHSELSEQ4.bit.CONV14 = cfg->channels[14];
    if (cfg->num_channels > 15U) AdcRegs.ADCCHSELSEQ4.bit.CONV15 = cfg->channels[15];

    /* ADCTRL2: all trigger/interrupt sources disabled at init.
     * EPWM_SOCA_SEQ1 and INT_ENA_SEQ1 are enabled by BSP gate after
     * both ADC + ePWM drivers are fully initialized. */
    AdcRegs.ADCTRL2.all = 0U;

    return 0;
}

int32_t DrvAdc_ReadRaw(uint32_t channel)
{
    if (channel >= 16U)
        return -1;

    /* PF0 mirror: zero wait states and right-justified 12-bit results. */
    return (int32_t)((&AdcMirror.ADCRESULT0)[channel]);
}

void DrvAdc_ClearInterrupt(void)
{
    AdcRegs.ADCST.bit.INT_SEQ1_CLR = 1U;
}

void DrvAdc_ResetSequencer(void)
{
    AdcRegs.ADCTRL2.bit.RST_SEQ1 = 1U;
}

void DrvAdc_EnableTrigger(void)
{
#if BOARD_PWM_ADC_HW_CONFIRMED == 1U
    AdcRegs.ADCTRL2.bit.EPWM_SOCA_SEQ1 = 1U;
    AdcRegs.ADCTRL2.bit.INT_ENA_SEQ1   = 1U;
#endif
}

void DrvAdc_AckInterrupt(void)
{
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
}
