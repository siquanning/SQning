/* Created by Siquanning */
#include "firmware/app/app.h"
#include "firmware/bsp/board.h"
#include "firmware/bsp/board_config.h"
#include "firmware/drivers/drv_timer.h"
#include "firmware/drivers/drv_spi.h"
#include "firmware/drivers/drv_epwm.h"
#include "firmware/drivers/drv_gpio.h"
#include "firmware/drivers/drv_interrupt.h"
#include "firmware/services/indicator.h"
#include "firmware/services/cpld_spi.h"
#include "firmware/services/modbus_vdc.h"
#include "firmware/services/justfloat.h"
#include "firmware/services/measurement.h"
#include "firmware/services/pll_host_protocol.h"
#include "firmware/app/isr.h"
#include "firmware/app/scheduler.h"
#include "firmware/app/run_control.h"
#include "firmware/app/run_supervisor.h"
#include "firmware/app/diagnostics.h"
#include "firmware/app/debug_snapshot.h"
#include "firmware/control/control_openloop.h"
#include "firmware/control/control_pll.h"
#include "firmware/control/control_global.h"
#include "firmware/control/control_closedloop.h"

/*
 * Internal Scheduler — owned exclusively by the App layer.
 * Sole writer: App_RunForever (foreground loop).
 * Sole reader of miss counters: App_Service100ms (via Scheduler_GetDiagnostics).
 */
static Scheduler g_sched;
static RunControl g_run_ctrl;         /* GPIO21 消抖稳定电平 (纯逻辑) */
static RunSupervisor g_run_sup;       /* 启停裁决: 抑制锁存 + PWM/LED 控制 */

/* GPIO21 消抖稳定电平观测镜像（DebugSnapshot 读取；纯观测，不参与裁决） */
volatile uint16_t g_run_request = 0U;

/* ===================================================================
 * App_Init
 * =================================================================== */
void App_Init(AppContext *app)
{
    uint32_t now;
    uint32_t init_diag_flags = 0UL;

    Board_Init();
    Measurement_Init();                /* 加载12路独立ADC offset默认值 */
    RunControl_Init(&g_run_ctrl);      /* stable=0 (STOP 请求) */
    RunSupervisor_Init(&g_run_sup);    /* restart_inhibit=1: 上电必须先看到 GPIO21=0 */

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
    PLL_Init(&g_pll);               /* PLL 状态清零, theta=0, freq=50Hz */
    ClosedLoop_Init();              /* 单相双闭环状态清零，等待RUN前锁存A/B/C */

    DrvInterrupt_BindEpwm1(&App_Epwm1Isr);
    DrvInterrupt_EnableEpwm1();                       /* PIE group 3 */
    DrvEpwm_EnablePeriodInt(BOARD_EPWM_MODULE);       /* ePWM INTEN=1 */

    /*
     * 每相两个ePWM共用相同TZ1/TZ2源；只给每相代表模块进入PIE，避免同一次
     * 共享Trip产生两个待处理中断。三相均绑定同一个全局安全入口。
     */
    DrvInterrupt_BindEpwmTz(1U, &App_EpwmTzIsr);     /* A: ePWM1/2 */
    DrvInterrupt_BindEpwmTz(3U, &App_EpwmTzIsr);     /* B: ePWM3/4 */
    DrvInterrupt_BindEpwmTz(5U, &App_EpwmTzIsr);     /* C: ePWM5/6 */
    DrvInterrupt_EnableEpwmTz(1U);
    DrvInterrupt_EnableEpwmTz(3U);
    DrvInterrupt_EnableEpwmTz(5U);                   /* PIE group 2 */

#if BOARD_DEBUG_JUSTFLOAT_ENABLE
    /* SCI-C TX FIFO 中断（PIE 8.6）：JustFloat 非阻塞发送续搬剩余字节 */
    DrvInterrupt_BindScicTx(&JustFloat_ScicTxIsr);
    DrvInterrupt_EnableScicTx();
#endif

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
    init_diag_flags |= DIAG_FLAG_PWM_ADC_HW_UNCONFIRMED;
#endif
    /*
     * INIT → STANDBY transition:
     * diag_flags with MSB (0x80000000) clear means no self-test failure detected.
     * The DIAG_FLAG_PWM_ADC_HW_UNCONFIRMED bit (0x00000001) does NOT block this transition —
     * it only records that PWM/ADC hardware has not been confirmed.
     *
     * Call StateMachine_Service twice: first call BOOT→INIT, second call INIT→STANDBY.
     */
    StateMachine_Service(&app->state_machine, now, init_diag_flags);  /* BOOT → INIT */
    StateMachine_Service(&app->state_machine, now, init_diag_flags);  /* INIT → STANDBY */

    /*
     * 上电保持 STANDBY — 不自动进入 RUN。
     * restart_inhibit=1 (RunSupervisor_Init): 必须先观察到 GPIO21 稳定为 0,
     * 之后 0→1 才由 RunSupervisor 请求 RUN 并释放 PWM。
     * PWM 上电即被 TZ 封锁, 保持到首次明确启动动作。
     */
}

/* ===================================================================
 * App_ServiceForeground — per-iteration foreground work
 * =================================================================== */
void App_ServiceForeground(AppContext *app, uint32_t now)
{
#if BOARD_DEBUG_JUSTFLOAT_ENABLE
    PllHostProtocol_Service(&app->pll_host_protocol, &app->sci_rx_queue);
#else
    ModbusVdc_Poll(&app->sci_rx_queue, now);
#endif
    Indicator_Service(now);
}

/* ===================================================================
 * App_Service1ms — parameter commit + PWM disable consumption
 * =================================================================== */
void App_Service1ms(AppContext *app, uint32_t now)
{
    /*
     * Parameter commit: ParamManager internally checks commit_requested,
     * validates, commits, and updates diagnostics. App layer never
     * reads commit_requested or writes commit/reject counters.
     */
    Param_ServicePendingCommit(&app->param_manager);
    PllHostProtocol_CommitPending(&app->pll_host_protocol);

    /*
     * Raw → physical conversion at 1 kHz (debug/CCS path).
     * Reusable conversion functions allow future 20 kHz fast-control
     * integration without duplicating the gain derivation.
     */
    Measurement_Update(&g_measurement);

    ClosedLoop_SlowStepAll(
        (StateMachine_IsRun(&app->state_machine) &&
         (g_pll_switch_req != 0U) &&
         (g_switch_alpha >= g_pll_ready_alpha_min)) ? 1U : 0U,
        0.001f);

    /* 1ms precharge voltage/timeout service; never runs in the 20kHz ISR. */
    RunSupervisor_Service1ms(&g_run_sup, &app->state_machine, now);

    /*
     * PWM disable: atomically consume the ISR/fault-path request flag.
     * The Consume function disables interrupts around the read-and-clear
     * on C2000 to prevent losing a concurrent System_EnterFault request.
     *
     * 统一走 PWM_BlockOutput (完整安全封锁入口: GPIO30 先拉低 + 全模块 TZ OST)。
     * FAULT 可在 ISR 触发, 此处保证 ≤1ms 内 PWM 封锁 + GPIO20 LED 熄灭。
     * 正常 STOP 不经过此路径 — 由 10ms RunSupervisor 同拍直接封锁。
     */
    if (StateMachine_ConsumePwmDisableRequest(&app->state_machine))
    {
        PWM_BlockOutput();
        DrvGpio_WriteRunState(0U);
    }

#if BOARD_DEBUG_JUSTFLOAT_ENABLE
    /* 安全/控制任务全部完成后再发送，250Hz JustFloat不得延迟本拍PWM封锁。 */
    JustFloat_Service();
#endif
}

/* ===================================================================
 * App_Service10ms — state machine service + fault debounce
 * =================================================================== */
void App_Service10ms(AppContext *app, uint32_t now)
{
    uint32_t diag_flags = 0UL;

    /* ---- 启停: GPIO21 保持型按钮, 消抖稳定电平 = 用户运行请求 ----
     * 稳定 1 = RUN 请求 (仅 STANDBY 且 TZ1/TZ2 正常时生效)
     * 稳定 0 = STOP 请求 (RUN 中同拍直接封锁 PWM + LED 灭 + 回 STANDBY)
     * FAULT / 重启抑制期间无论电平如何都保持封锁, 详见 RunSupervisor。
     */
    {
        uint16_t active = (DrvGpio_ReadRunButton() == BOARD_RUN_BTN_ACTIVE_LEVEL)
                        ? 1U : 0U;

        RunControl_Sample(&g_run_ctrl, active);
        g_run_request = RunControl_GetStableLevel(&g_run_ctrl);
        RunSupervisor_Service(&g_run_sup, &app->state_machine,
                              g_run_request, now);
    }

    /* ---- PLL 锁定判决 → 软切换请求 ----
     * 锁定: 连续 LOCK_TICKS 拍全部满足 → req=1 (α 淡入 PLL)
     * 失锁: 连续 UNLOCK_TICKS 拍任一不满足 → req=0 (α 淡出回开环)
     * 迟滞不对称: 锁上要慢(防抖), 撤出要快。
     */
    {
        static uint16_t s_lock_ctr   = 0U;
        static uint16_t s_unlock_ctr = 0U;
        PLL_State p;
        PLL_Params pll_params;
        float vq_ratio_limit;
        uint16_t ok;

        DrvInterrupt_DisableGlobal();
        p = g_pll;
        DrvInterrupt_RestoreGlobal();
        PLL_ReadActiveParams(&pll_params);
        vq_ratio_limit = (g_pll_switch_req != 0U)
                       ? pll_params.vq_unlock_ratio
                       : pll_params.vq_lock_ratio;

        ok = (p.vmag > BOARD_PLL_LOCK_VMAG_MIN_V)
          && (p.freq >= BOARD_PLL_LOCK_FREQ_MIN_HZ)
          && (p.freq <= BOARD_PLL_LOCK_FREQ_MAX_HZ)
          && (p.vq > -(vq_ratio_limit * p.vmag))
          && (p.vq <   vq_ratio_limit * p.vmag)
          && (p.vd >   BOARD_PLL_LOCK_VD_RATIO * p.vmag)
          ? 1U : 0U;

        if (ok != 0U) {
            s_unlock_ctr = 0U;
            if (s_lock_ctr < BOARD_PLL_LOCK_TICKS) s_lock_ctr++;
            if (s_lock_ctr >= BOARD_PLL_LOCK_TICKS) g_pll_switch_req = 1U;
        } else {
            s_lock_ctr = 0U;
            if (s_unlock_ctr < BOARD_PLL_UNLOCK_TICKS) s_unlock_ctr++;
            if (s_unlock_ctr >= BOARD_PLL_UNLOCK_TICKS) {
                /* 先完成硬件封锁，再撤销软件闭环许可，消除m=0过渡窗口。 */
                if ((g_pll_switch_req != 0U) &&
                    StateMachine_IsRun(&app->state_machine)) {
                    PWM_BlockOutput();
                    ClosedLoop_ClearActiveConfig();
                    System_EnterFault(&app->state_machine,
                                      FAULT_HW_PLL_LOCK_LOST, now);
                    DrvGpio_WriteRunState(0U);
                }
                g_pll_switch_req = 0U;
            }
        }
    }

    /*
     * PLL失锁消抖确认后，m=0不足以表示硬件关闭：立即复用全局OST入口，
     * 并锁存PLL失锁FAULT。非法活动相同样不得维持任何PWM释放。
     */
    if (StateMachine_IsRun(&app->state_machine) &&
        ((g_pll_switch_req == 0U) ||
         (ClosedLoop_IsValidRunMode(ClosedLoop_GetActiveRunMode()) == 0U) ||
         ((ClosedLoop_GetActiveRunMode() == CTRL_RUN_MODE_SINGLE_PHASE) &&
          (ClosedLoop_IsValidTestPhase(ClosedLoop_GetActivePhase()) == 0U)))) {
        PWM_BlockOutput();
        ClosedLoop_ClearActiveConfig();
        System_EnterFault(&app->state_machine,
                          (g_pll_switch_req == 0U)
                          ? FAULT_HW_PLL_LOCK_LOST
                          : FAULT_SW_CONTROL_INVALID,
                          now);
        DrvGpio_WriteRunState(0U);
    }

#if BOARD_PWM_ADC_HW_CONFIRMED == 0U
    diag_flags |= DIAG_FLAG_PWM_ADC_HW_UNCONFIRMED;
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
    diag_flags |= DIAG_FLAG_PWM_ADC_HW_UNCONFIRMED;
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
            static uint32_t cpld_led_tick = 0UL;

            App_Service100ms(app, &g_sched, now);

            if (++cpld_led_tick >= 10UL)
            {
                cpld_led_tick = 0UL;
                DrvGpio_ToggleCpldLed();
            }
        }
    }
}
