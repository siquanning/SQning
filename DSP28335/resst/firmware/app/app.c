#include "firmware/app/app.h"
#include "firmware/bsp/board.h"
#include "firmware/bsp/board_config.h"
#include "firmware/drivers/drv_timer.h"
#include "firmware/drivers/drv_spi.h"
#include "firmware/drivers/drv_epwm.h"
#include "firmware/drivers/drv_interrupt.h"
#include "firmware/services/indicator.h"
#include "firmware/services/cpld_spi.h"
#include "firmware/services/modbus_vdc.h"
#include "firmware/services/measurement.h"
#include "firmware/app/isr.h"
#include "firmware/app/scheduler.h"
#include "firmware/app/diagnostics.h"
#include "firmware/control/control_openloop.h"

/*
 * Internal Scheduler — owned exclusively by the App layer.
 * Sole writer: App_RunForever (foreground loop).
 * Sole reader of miss counters: App_Service100ms (via Scheduler_GetDiagnostics).
 */
static Scheduler g_sched;

/* ===================================================================
 * App_Init
 * =================================================================== */
void App_Init(AppContext *app)
{
    uint32_t now;
    uint32_t init_diag_flags = 0UL;

    Board_Init();
    Diagnostics_Init();

    AppContext_Init(app);

    /* Wire ISR pointers */
    App_IsrSetQueue(&app->sci_rx_queue);
    App_IsrSetControl(&app->control);
    App_IsrSetStateMachine(&app->state_machine);
    App_IsrSetParamManager(&app->param_manager);
    App_IsrSetTelemetry(&app->telemetry);

    /* ---- 20 kHz EPWM1 Fast ISR: 50 Hz 3-phase clamped-unipolar ---- */
    OpenLoop_InitSine();             /* 1200-point Q15 LUT + phase=0 */

    DrvInterrupt_BindEpwm1(&App_Epwm1Isr);
    DrvInterrupt_EnableEpwm1();                       /* PIE group 3 */
    DrvEpwm_EnablePeriodInt(BOARD_EPWM_MODULE);       /* ePWM INTEN=1 */

    PWM_BlockOutput();        /* TZ safety block — outputs stay LOW */
    PWM_StartTimebase();      /* TBCLKSYNC=1 → counters run → ISR fires */

    /*
     * ISR is now running at 20 kHz.  CMP + AQCSFRC shadows are prepared
     * each cycle and load at the next CTR=ZERO.  Outputs remain blocked
     * by TZ until PWM_ReleaseOutput() is called (manual enable).
     */

    Indicator_Init();
    CPLD_Init();
    ModbusVdc_Init();

    now = Timebase_Now();
    Scheduler_Init(&g_sched, now);

    /* BOOT → INIT (StateMachine_Init sets BOOT, first Service call transitions to INIT) */
    StateMachine_Init(&app->state_machine, now);

#if BOARD_PWM_ADC_HW_CONFIRMED == 0U
    init_diag_flags |= DIAG_FLAG_LOGICAL_RUN_NO_HW;
#endif
    /*
     * INIT → STANDBY transition:
     * diag_flags with MSB (0x80000000) clear means no self-test failure detected.
     * The DIAG_FLAG_LOGICAL_RUN_NO_HW bit (0x00000001) does NOT block this transition —
     * it only records that PWM/ADC hardware has not been confirmed.
     *
     * Call StateMachine_Service twice: first call BOOT→INIT, second call INIT→STANDBY.
     */
    StateMachine_Service(&app->state_machine, now, init_diag_flags);  /* BOOT → INIT */
    StateMachine_Service(&app->state_machine, now, init_diag_flags);  /* INIT → STANDBY */

    /* Request RUN — enters logical RUN (PWM stays disabled when HW_CONFIRMED=0) */
    StateMachine_RequestRun(&app->state_machine, now);
}

/* ===================================================================
 * App_ServiceForeground — per-iteration foreground work
 * =================================================================== */
void App_ServiceForeground(AppContext *app, uint32_t now)
{
    ModbusVdc_Poll(&app->sci_rx_queue, now);
    Indicator_Service(now);
}

/* ===================================================================
 * App_Service1ms — parameter commit + PWM disable consumption
 * =================================================================== */
void App_Service1ms(AppContext *app, uint32_t now)
{
    (void)now;

    /*
     * Parameter commit: ParamManager internally checks commit_requested,
     * validates, commits, and updates diagnostics. App layer never
     * reads commit_requested or writes commit/reject counters.
     */
    Param_ServicePendingCommit(&app->param_manager);

    /*
     * Raw → physical conversion at 1 kHz (debug/CCS path).
     * Reusable conversion functions allow future 20 kHz fast-control
     * integration without duplicating the gain derivation.
     */
    Measurement_Update(&g_measurement);

    /*
     * PWM disable: atomically consume the ISR/fault-path request flag.
     * The Consume function disables interrupts around the read-and-clear
     * on C2000 to prevent losing a concurrent System_EnterFault request.
     */
    if (StateMachine_ConsumePwmDisableRequest(&app->state_machine))
    {
        PWM_Disable();
    }
}

/* ===================================================================
 * App_Service10ms — state machine service + fault debounce
 * =================================================================== */
void App_Service10ms(AppContext *app, uint32_t now)
{
    uint32_t diag_flags = 0UL;

#if BOARD_PWM_ADC_HW_CONFIRMED == 0U
    diag_flags |= DIAG_FLAG_LOGICAL_RUN_NO_HW;
#endif

    StateMachine_Service(&app->state_machine, now, diag_flags);
}

/* ===================================================================
 * App_Service100ms — diagnostics, telemetry, fault detection
 * =================================================================== */
void App_Service100ms(AppContext *app, Scheduler *sched, uint32_t now)
{
#if BOARD_CPLD_PROTOCOL_TEST
    /*
     * A相测试：周期发送调制量 + H1透传/H2使能，同时刷新CPLD看门狗。
     * SPI字节: 02 00 00 C8, 02 03 00 03
     */
    CPLD_WriteReg16(0U, (int16_t)200);  /* A相调制量 +0.2 */
    CPLD_WriteReg16(3U, (int16_t)3);    /* H1透传 + H2使能 */
#endif

    Diagnostics *diag = Diagnostics_Get();
    ControlParams active_params;
    uint32_t miss1, miss10, miss100;
    uint32_t sci_overflow;
    uint16_t sm_state, sm_fault_code;
    uint32_t sm_fault_tick;
    uint32_t pm_commits, pm_rejects;
    uint16_t pm_last_reject;
    uint32_t telem_writes, telem_overruns;
    uint32_t diag_flags = 0UL;

    /* Read active parameter set once for fault threshold checks */
    Param_ReadActive(&app->param_manager, &active_params);

    /* ---- Scheduler diagnostics ---- */
    Scheduler_GetDiagnostics(sched, &miss1, &miss10, &miss100);
    sci_overflow = SciRxQueue_GetOverflowCount(&app->sci_rx_queue);
    Diagnostics_SetSchedulerStats(diag, miss1, miss10, miss100, sci_overflow);

    /* ---- State machine diagnostics ---- */
    StateMachine_GetDiagSnapshot(&app->state_machine,
                                 &sm_state, &sm_fault_code, &sm_fault_tick);
    Diagnostics_SetSystemDiag(diag, sm_state, sm_fault_code, sm_fault_tick);

    /* ---- Parameter diagnostics ---- */
    Param_GetDiagSnapshot(&app->param_manager,
                          &pm_commits, &pm_rejects, &pm_last_reject);
    Diagnostics_SetParamStats(diag, pm_commits, pm_rejects, pm_last_reject);

    /* ---- Telemetry snapshot ---- */
    {
        TelemetryFastSnapshot telem_snap;
        Telemetry_ReadSnapshot(&app->telemetry, &telem_snap);
        telem_writes  = app->telemetry.write_count;
        telem_overruns = app->telemetry.overrun_count;
    }
    Diagnostics_SetTelemetryStats(diag, telem_writes, telem_overruns);

    /* ---- PWM diagnostic snapshots ---- */
    Diagnostics_SetPwmSnapshot(diag,
                               DrvEpwm_GetPeriod(BOARD_EPWM_MODULE),
                               DrvEpwm_GetCounter(BOARD_EPWM_MODULE));

    /* ---- diag_flags ---- */
#if BOARD_PWM_ADC_HW_CONFIRMED == 0U
    diag_flags |= DIAG_FLAG_LOGICAL_RUN_NO_HW;
#endif
    Diagnostics_WriteDiagFlags(diag, diag_flags);

    /* ---- System fault detection: scheduler miss ---- */
    if (miss10 > active_params.fault_thresh_sched_miss)
    {
        System_EnterFault(&app->state_machine,
                          FAULT_SYS_SCHEDULER_MISS, now);
    }

    /* ---- System fault detection: SPI timeout ---- */
    {
        SpiRequestDiagnostics spi_diag;
        SpiBridge_GetSpiDiagnostics(&app->spi_bridge, &spi_diag);
        if (spi_diag.timeouts > (uint32_t)active_params.fault_thresh_spi_timeout)
        {
            System_EnterFault(&app->state_machine,
                              FAULT_COMM_SPI_TIMEOUT_EXCESSIVE, now);
        }
    }
}

/* ===================================================================
 * App_RunForever
 * =================================================================== */
void App_RunForever(AppContext *app)
{
    uint32_t prev = Diagnostics_CycleRead();

    for (;;)
    {
        uint32_t now = Timebase_Now();

        Diagnostics_WcetUpdate(&Diagnostics_Get()->main_loop,
                               prev - Diagnostics_CycleRead());
        prev = Diagnostics_CycleRead();

        /* Foreground: SCI drain + SPI bridge + indicator */
        App_ServiceForeground(app, now);

        /* ---- 1ms: parameter commit + PWM disable consumption ---- */
        if (Scheduler_Take1ms(&g_sched, now))
        {
            App_Service1ms(app, now);
        }

        /* ---- 10ms: state machine + fault debounce ---- */
        if (Scheduler_Take10ms(&g_sched, now))
        {
            App_Service10ms(app, now);
        }

        /* ---- 100ms: diagnostics, telemetry, fault detection ---- */
        if (Scheduler_Take100ms(&g_sched, now))
        {
            App_Service100ms(app, &g_sched, now);
        }
    }
}
