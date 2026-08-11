#include "firmware/drivers/drv_timer.h"
#include "firmware/drivers/drv_sci.h"
#include "firmware/drivers/drv_adc.h"
#include "firmware/drivers/drv_epwm.h"
#include "firmware/drivers/drv_interrupt.h"
#include "firmware/bsp/board_config.h"
#include "firmware/control/control_faststep.h"
#include "firmware/control/control_openloop.h"
#include "firmware/app/isr.h"
#include "firmware/app/diagnostics.h"

static SciRxQueue    *g_pSciRxQueue   = ((SciRxQueue *)0);
static ControlContext *g_pControl      = ((ControlContext *)0);
static StateMachine  *g_pStateMachine = ((StateMachine *)0);
static ParamManager  *g_pParamManager = ((ParamManager *)0);
static Telemetry     *g_pTelemetry    = ((Telemetry *)0);

volatile uint16_t g_vdc_raw[6];
volatile uint16_t g_vac_raw[3];
volatile uint16_t g_iac_raw[3];
volatile uint32_t g_adc_frame_count;

void App_IsrSetQueue(SciRxQueue *queue)
{
    g_pSciRxQueue = queue;
}

void App_IsrSetControl(ControlContext *control)
{
    g_pControl = control;
}

void App_IsrSetStateMachine(StateMachine *sm)
{
    g_pStateMachine = sm;
}

void App_IsrSetParamManager(ParamManager *pm)
{
    g_pParamManager = pm;
}

void App_IsrSetTelemetry(Telemetry *t)
{
    g_pTelemetry = t;
}

__interrupt void App_Timer0Isr(void)
{
    uint32_t t0 = Diagnostics_CycleRead();

    DrvTimer0_OnInterrupt();
    DrvInterrupt_AckGroup1();

    Diagnostics_WcetUpdate(&Diagnostics_Get()->timer0_isr,
                           t0 - Diagnostics_CycleRead());
}

__interrupt void App_ScicRxIsr(void)
{
    uint32_t t0 = Diagnostics_CycleRead();
    uint32_t now = Timebase_Now();
    uint16_t errFlags;
    int      hadError = 0;

    if (g_pSciRxQueue == ((SciRxQueue *)0))
    {
        DrvSci_ClearRxInterrupt();
        DrvInterrupt_AckGroup8();
        Diagnostics_WcetUpdate(&Diagnostics_Get()->sci_rx_isr,
                               t0 - Diagnostics_CycleRead());
        return;
    }

    Diagnostics_Get()->sci_rx_total++;

    if (DrvSci_HasError(&errFlags))
    {
        hadError = 1;
        SciRxQueue_PushFromIsr(g_pSciRxQueue, 0U, errFlags, now);
    }
    else
    {
        uint16_t fifoCount = DrvSci_GetRxFifoCount();
        uint16_t i;

        for (i = 0U; i < fifoCount; i++)
        {
            uint16_t rxStCurr = DrvSci_GetStatus();
            if (DrvSci_StatusHasError(rxStCurr))
            {
                hadError = 1;
                SciRxQueue_PushFromIsr(g_pSciRxQueue, 0U,
                                       DrvSci_GetErrorFromStatus(rxStCurr), now);
                break;
            }
            uint16_t b = DrvSci_ReadByte();
            SciRxQueue_PushFromIsr(g_pSciRxQueue, b, 0U, now);
        }
    }

    if (hadError)
    {
        DrvSci_RecoverRx();
    }

    DrvSci_ClearRxInterrupt();
    DrvInterrupt_AckGroup8();

    Diagnostics_WcetUpdate(&Diagnostics_Get()->sci_rx_isr,
                           t0 - Diagnostics_CycleRead());
}

/*
 * App_AdcIsr — 12-channel ADC capture (6×Vdc + 3×Vac + 3×Iac).
 * Reads the zero-wait-state, right-justified ADC mirror registers through the
 * ADC driver and stores them in g_vdc_raw[0..5], g_vac_raw[0..2],
 * g_iac_raw[0..2].
 * No filtering, scaling, UART, Modbus, or protection.
 * Triggered by EPWM1 SOCA at CTR=ZERO (20 kHz).
 *
 * ADCRESULT0→Vdc1,…,ADCRESULT5→Vdc6,
 * ADCRESULT6→Va, ADCRESULT7→Vb, ADCRESULT8→Vc,
 * ADCRESULT9→Ia, ADCRESULT10→Ib, ADCRESULT11→Ic.
 */
__interrupt void App_AdcIsr(void)
{
    g_vdc_raw[0] = (uint16_t)DrvAdc_ReadRaw(0U);
    g_vdc_raw[1] = (uint16_t)DrvAdc_ReadRaw(1U);
    g_vdc_raw[2] = (uint16_t)DrvAdc_ReadRaw(2U);
    g_vdc_raw[3] = (uint16_t)DrvAdc_ReadRaw(3U);
    g_vdc_raw[4] = (uint16_t)DrvAdc_ReadRaw(4U);
    g_vdc_raw[5] = (uint16_t)DrvAdc_ReadRaw(5U);
    g_vac_raw[0] = (uint16_t)DrvAdc_ReadRaw(6U);
    g_vac_raw[1] = (uint16_t)DrvAdc_ReadRaw(7U);
    g_vac_raw[2] = (uint16_t)DrvAdc_ReadRaw(8U);
    g_iac_raw[0] = (uint16_t)DrvAdc_ReadRaw(9U);   /* Ia */
    g_iac_raw[1] = (uint16_t)DrvAdc_ReadRaw(10U);  /* Ib */
    g_iac_raw[2] = (uint16_t)DrvAdc_ReadRaw(11U);  /* Ic */

    g_adc_frame_count++;

    DrvAdc_ResetSequencer();
    DrvAdc_ClearInterrupt();
    DrvAdc_AckInterrupt();
}

/*
 * EPWM1 period ISR — 20 kHz clamped-unipolar modulation for H1.
 *
 * Architecture: 3 phases × 2 half-bridges = 6 ePWM modules.
 * Each ePWM module uses CMPA only; A/B outputs are strictly complementary.
 *
 *   Phase N left  → ePWM(2N-1): A=upper, B=lower
 *   Phase N right → ePWM(2N):   A=upper, B=lower
 *
 * Triggered by CTR=ZERO on EPWM1.  Shadows load at next CTR=ZERO
 * → one-cycle pipeline delay (50 us).
 */
__interrupt void App_Epwm1Isr(void)
{
    uint32_t      t0   = Diagnostics_CycleRead();
    Diagnostics  *diag = Diagnostics_Get();
    int16_t       mabc[3];
    uint16_t      phase;

    static const uint32_t s_left_module[3]  = { 1U, 3U, 5U };
    static const uint32_t s_right_module[3] = { 2U, 4U, 6U };

    diag->fast_isr_count++;

    /* 50 Hz 3-phase sine reference — 1200-point LUT, 20000/400=50Hz */
    OpenLoop_GenerateSine(mabc);

    /* Mirror to ControlContext for diagnostics / telemetry visibility */
    if (g_pControl != ((ControlContext *)0))
    {
        g_pControl->m_permill[0] = mabc[0];
        g_pControl->m_permill[1] = mabc[1];
        g_pControl->m_permill[2] = mabc[2];
    }

    for (phase = 0U; phase < 3U; phase++)
    {
        PhasePwmCommand cmd;

        Control_ComputeModulation(
            mabc[phase],
            (uint16_t)BOARD_PWM_TBPRD,
            &cmd);

        /* Left half-bridge */
        if (cmd.left.force_high == 0U)
        {
            DrvEpwm_SetCompareA(
                s_left_module[phase],
                cmd.left.cmp,
                BOARD_MODULATION_DUTY_MAX_PERMILL);
        }

        DrvEpwm_SetHalfBridgeForceHigh(
            s_left_module[phase],
            cmd.left.force_high);

        /* Right half-bridge */
        if (cmd.right.force_high == 0U)
        {
            DrvEpwm_SetCompareA(
                s_right_module[phase],
                cmd.right.cmp,
                BOARD_MODULATION_DUTY_MAX_PERMILL);
        }

        DrvEpwm_SetHalfBridgeForceHigh(
            s_right_module[phase],
            cmd.right.force_high);
    }

    DrvEpwm_ClearIntFlag(BOARD_EPWM_MODULE);
    DrvInterrupt_AckGroup3();

    Diagnostics_WcetUpdate(&diag->fast_isr,
                           t0 - Diagnostics_CycleRead());
}
