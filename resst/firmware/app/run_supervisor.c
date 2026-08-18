/* Created by Siquanning */
#include "firmware/app/run_supervisor.h"
#include "firmware/bsp/board.h"
#include "firmware/bsp/board_config.h"
#include "firmware/drivers/drv_gpio.h"
#include "firmware/services/measurement.h"
#include "firmware/control/control_global.h"
#include "firmware/control/control_closedloop.h"
#include "firmware/drivers/drv_interrupt.h"

#define START_SEQ_TICKS_PER_MS (10UL)

/*
 * 六路直流电压中的最低值达到该门槛后，才认为六个单元均完成基本预充。
 * 单位：V（以g_measurement.vdc_v[]当前标定结果为准）。
 * 现场临时调试：CCS Expressions修改g_precharge_done_v。
 * DSP复位后恢复为BOARD_PRECHARGE_DONE_V_DEFAULT。
 */
volatile float g_precharge_done_v;

/* 预充最长允许时间，单位ms；超时后切断输入并要求GPIO21回0后才能重试。 */
volatile uint32_t g_precharge_timeout_ms;

/* GPIO23闭合、旁路预充电阻后继续保持PWM封锁的等待时间，单位ms。 */
volatile uint32_t g_bypass_delay_ms;

/* PLL软切换alpha的启动许可下限；只决定是否允许启动，不修改PLL内部算法。 */
volatile float g_pll_ready_alpha_min;

volatile uint16_t g_grid_switch_cmd   = 0U;
volatile uint16_t g_bypass_switch_cmd = 0U;
volatile uint16_t g_start_seq_fail    = 0U;
volatile float    g_precharge_vdc_min  = 0.0f;
volatile uint16_t g_start_seq_state    = (uint16_t)START_SEQ_IDLE;
volatile uint32_t g_start_seq_timer_ms = 0UL;

static void StartSeq_WriteGridSwitch(uint16_t on)
{
    /* GPIO22：三相输入总开关，同时控制S1/S2/S3。 */
    g_grid_switch_cmd = (on != 0U) ? 1U : 0U;
    DrvGpio_WriteGridSwitch(g_grid_switch_cmd);
}

static void StartSeq_WriteBypass(uint16_t on)
{
    /* GPIO23：预充电阻旁路总开关，同时控制S4/S5/S6。 */
    g_bypass_switch_cmd = (on != 0U) ? 1U : 0U;
    DrvGpio_WritePrechargeBypass(g_bypass_switch_cmd);
}

static void StartSeq_Reset(RunSupervisor *rs)
{
    rs->seq_state = START_SEQ_IDLE;
    rs->seq_entry_tick = 0UL;
    g_start_seq_state = (uint16_t)START_SEQ_IDLE;
    g_start_seq_timer_ms = 0UL;
}

/*
 * 只负责断开功率路径。调用者必须已经先执行PWM_BlockOutput()；
 * GPIO23先断可使预充电阻退出旁路，随后GPIO22断开三相输入。
 * 该顺序属于安全要求，禁止把本函数移到PWM封锁之前。
 */
static void StartSeq_OpenPowerPath(RunSupervisor *rs)
{
    ClosedLoop_ClearActiveConfig();
    StartSeq_WriteBypass(0U);      /* GPIO23 OFF first */
    StartSeq_WriteGridSwitch(0U);  /* GPIO22 OFF second */
    StartSeq_Reset(rs);
}

#if (BOARD_PLL_RELAY_TEST_ONLY == 0U) && (BOARD_OPENLOOP_SPWM_TEST == 0U)
static uint16_t StartSeq_PllReady(void)
{
    uint16_t req;
    float alpha;

    DrvInterrupt_DisableGlobal();
    req = g_pll_switch_req;
    alpha = g_switch_alpha;
    DrvInterrupt_RestoreGlobal();

    /* 既要通过原PLL锁定消抖，也要让LUT到PLL的相位软切换接近完成。 */
    return ((req != 0U) && (alpha >= g_pll_ready_alpha_min)) ? 1U : 0U;
}
#endif

static uint32_t StartSeq_ElapsedMs(uint32_t now, uint32_t entry_tick)
{
    return (now - entry_tick) / START_SEQ_TICKS_PER_MS;
}

void RunSupervisor_Init(RunSupervisor *rs)
{
    if (rs == ((RunSupervisor *)0)) return;

    rs->restart_inhibit = 1U;
    StartSeq_Reset(rs);

    /* DSP复位/应用初始化后，现场运行参数恢复为board_config.h默认值。 */
    g_precharge_done_v = BOARD_PRECHARGE_DONE_V_DEFAULT;
    g_precharge_timeout_ms = BOARD_PRECHARGE_TIMEOUT_MS_DEFAULT;
    g_bypass_delay_ms = BOARD_BYPASS_DELAY_MS_DEFAULT;
    g_pll_ready_alpha_min = BOARD_PLL_READY_ALPHA_MIN_DEFAULT;

    g_grid_switch_cmd = 0U;
    g_bypass_switch_cmd = 0U;
    g_start_seq_fail = 0U;
    g_precharge_vdc_min = 0.0f;
    ClosedLoop_ClearActiveConfig();

    /*
     * 初始化必须主动断开两组继电器，不能只把软件命令缓存清零。
     * BSP已在更早阶段预置GPIO42/44为LOW，这里再次写硬件确保应用层
     * 初始化/软复位后的实际输出与命令状态一致。
     */
    StartSeq_WriteBypass(0U);
    StartSeq_WriteGridSwitch(0U);
}

void RunSupervisor_Service(RunSupervisor *rs, StateMachine *sm,
                           uint16_t run_request, uint32_t now)
{
    if ((rs == ((RunSupervisor *)0)) || (sm == ((StateMachine *)0))) return;

#if (BOARD_CLOCK_BRINGUP_ONLY != 0U)
    /*
     * Clock Bring-up 模式：禁止进入任何真实功率 RUN 路径。
     *  - 保持 TZ OST Block + AQCSFRC 安全状态（PWM_BlockOutput）
     *  - GPIO42/44 强制安全 OFF（不吸合继电器）
     *  - GPIO30 恒 LOW（Release 被 board.c 阻断）
     *  - 状态机保持 STANDBY；即使 GPIO21 变化也不进入 RUN
     *  - 允许时基运行（TBCLKSYNC=1）与 Timer0 中断（诊断/5kHz 测试信号）
     */
    PWM_BlockOutput();
    StartSeq_WriteBypass(0U);      /* GPIO44 OFF */
    StartSeq_WriteGridSwitch(0U);  /* GPIO42 OFF */
    ClosedLoop_ClearActiveConfig();
    DrvGpio_WriteRunState(0U);
    if (StateMachine_IsRun(sm))
    {
        StateMachine_RequestStandby(sm, now);
    }
    (void)run_request;
    return;
#endif

    if (StateMachine_IsFault(sm))
    {
        rs->restart_inhibit = 1U;
        PWM_BlockOutput();            /* Safety order: PWM first */
        StartSeq_OpenPowerPath(rs);   /* GPIO23 then GPIO22 */
        DrvGpio_WriteRunState(0U);
        return;
    }

    if (rs->restart_inhibit != 0U)
    {
        PWM_BlockOutput();            /* Keep the complete power path safe */
        StartSeq_OpenPowerPath(rs);
        DrvGpio_WriteRunState(0U);
        if (run_request == 0U)
        {
            rs->restart_inhibit = 0U;
        }
        return;
    }

#if (BOARD_PLL_RELAY_TEST_ONLY != 0U)
    /*
     * PLL/继电器台架测试：无论PLL、TZ和运行模式如何都禁止进入RUN，
     * 并在处理继电器命令前再次实施完整硬件封锁。GPIO21只控制两组
     * 继电器同时吸合/释放；主回路接入高压时严禁使用此测试模式吸合。
     */
    PWM_BlockOutput();
    ClosedLoop_ClearActiveConfig();
    StartSeq_Reset(rs);
    StateMachine_RequestStandby(sm, now);
    /* 台架模式下GPIO20临时作为GPIO21/继电器动作指示：按下亮，松开灭。 */
    DrvGpio_WriteRunState(run_request);
    StartSeq_WriteGridSwitch(run_request);
    StartSeq_WriteBypass(run_request);
    return;
#else

    if (run_request == 0U)
    {
        if ((rs->seq_state != START_SEQ_IDLE) || StateMachine_IsRun(sm))
        {
            /*
             * GPIO21停止后的固定安全顺序：
             * 1. 先PWM_BlockOutput()，立即停止主动开关；
             * 2. 再关闭GPIO23，退出预充电阻旁路；
             * 3. 最后关闭GPIO22，切断三相输入。
             * 此顺序属于安全要求，禁止调整，也不得推迟到下一次1ms任务。
             */
            PWM_BlockOutput();
            StartSeq_WriteBypass(0U);      /* GPIO23: S4/S5/S6 open */
            StartSeq_WriteGridSwitch(0U);  /* GPIO22: S1/S2/S3 open */
            ClosedLoop_ClearActiveConfig();
            StartSeq_Reset(rs);
            StateMachine_RequestStandby(sm, now);
            DrvGpio_WriteRunState(0U);
        }
        return;
    }

    if (StateMachine_IsRun(sm))
    {
        return;
    }

    if (!StateMachine_IsStandby(sm))
    {
        return;
    }

    if (rs->seq_state == START_SEQ_IDLE)
    {
#if (BOARD_LOW_VOLTAGE_DIRECT_TEST != 0U)
        uint16_t active_mode;
        /*
         * 低压台架不做预充延时：合法测试相确认后，GPIO22/GPIO23同拍闭合，
         * PWM继续保持OST。随后等待PLL锁定和TZ正常，再释放目标相PWM。
         * 这样既满足“先22/23、后PWM”，也避免采样点位于开关后侧时PLL死等。
        */
        PWM_BlockOutput();
        active_mode = ClosedLoop_LatchRunConfig();
        if (ClosedLoop_IsValidRunMode(active_mode) == 0U) {
            ClosedLoop_ClearActiveConfig();
            StartSeq_WriteBypass(0U);
            StartSeq_WriteGridSwitch(0U);
            DrvGpio_WriteRunState(0U);
            return;
        }
        StartSeq_WriteGridSwitch(1U);
        StartSeq_WriteBypass(1U);
        rs->seq_state = START_SEQ_BYPASS_WAIT;
        rs->seq_entry_tick = now;
        g_start_seq_state = (uint16_t)START_SEQ_BYPASS_WAIT;
        g_start_seq_timer_ms = 0UL;
        g_start_seq_fail = 0U;
        DrvGpio_WriteRunState(0U);
#else
        uint16_t active_mode;
        /*
         * 投入GPIO22后由预充电阻和H桥反并联二极管进行不控整流预充。
         * 此阶段PLL继续在20kHz ISR后台锁相，但PWM必须保持封锁。
         * 与直测模式一致：进入启动序列即锁存相别/模式（RUN期间热改不生效）。
         */
        PWM_BlockOutput();
        active_mode = ClosedLoop_LatchRunConfig();
        if (ClosedLoop_IsValidRunMode(active_mode) == 0U) {
            ClosedLoop_ClearActiveConfig();
            StartSeq_WriteBypass(0U);
            StartSeq_WriteGridSwitch(0U);
            DrvGpio_WriteRunState(0U);
            return;
        }
        StartSeq_WriteBypass(0U);
        StartSeq_WriteGridSwitch(1U);
        rs->seq_state = START_SEQ_PRECHARGE;
        rs->seq_entry_tick = now;
        g_start_seq_state = (uint16_t)START_SEQ_PRECHARGE;
        g_start_seq_timer_ms = 0UL;
        g_start_seq_fail = 0U;
        DrvGpio_WriteRunState(0U);
#endif
        return;
    }

    if (rs->seq_state == START_SEQ_BYPASS_WAIT)
    {
        g_start_seq_timer_ms = StartSeq_ElapsedMs(now, rs->seq_entry_tick);
        if ((StartSeq_ElapsedMs(now, rs->seq_entry_tick) >=
#if (BOARD_LOW_VOLTAGE_DIRECT_TEST != 0U)
             0UL)
#else
             g_bypass_delay_ms)
#endif
#if (BOARD_OPENLOOP_SPWM_TEST == 0U)
            && (StartSeq_PllReady() != 0U)
#endif
            && (PWM_AreTripInputsClear() != 0U))
        {
            uint16_t active_mode = ClosedLoop_GetActiveRunMode();
            uint16_t active_phase = ClosedLoop_GetActivePhase();
            if ((ClosedLoop_IsValidRunMode(active_mode) != 0U) &&
                StateMachine_RequestRun(sm, now))
            {
#if (BOARD_OPENLOOP_SPWM_TEST != 0U)
                uint16_t released = PWM_ReleaseThreePhase();
                (void)active_phase;
#else
                uint16_t released = (active_mode == CTRL_RUN_MODE_THREE_PHASE)
                                  ? PWM_ReleaseThreePhase()
                                  : PWM_ReleaseSelectedPhase(active_phase);
#endif
                if (released != 0U) {
                    DrvGpio_WriteRunState(1U);
                    StartSeq_Reset(rs);
                } else {
                    PWM_BlockOutput();
#if (BOARD_LOW_VOLTAGE_DIRECT_TEST != 0U)
                    StartSeq_WriteBypass(0U);
                    StartSeq_WriteGridSwitch(0U);
#endif
                    ClosedLoop_ClearActiveConfig();
                    System_EnterFault(sm, FAULT_SW_CONTROL_INVALID, now);
                }
            }
        }
    }
#endif
}

void RunSupervisor_Service1ms(RunSupervisor *rs, StateMachine *sm, uint32_t now)
{
    float vdc_min;
    uint16_t i;

    if ((rs == ((RunSupervisor *)0)) || (sm == ((StateMachine *)0))) return;

#if (BOARD_CLOCK_BRINGUP_ONLY != 0U)
    /* Clock Bring-up 模式：1ms 路径持续重申封锁，防止任何后台路径释放功率。 */
    PWM_BlockOutput();
    StartSeq_WriteBypass(0U);      /* GPIO44 OFF */
    StartSeq_WriteGridSwitch(0U);  /* GPIO42 OFF */
    ClosedLoop_ClearActiveConfig();
    if (StateMachine_IsRun(sm)) StateMachine_RequestStandby(sm, now);
    return;
#endif

#if (BOARD_PLL_RELAY_TEST_ONLY != 0U)
    /* 台架模式持续重申硬件封锁，防止任何后台路径遗留PWM释放状态。 */
    PWM_BlockOutput();
    ClosedLoop_ClearActiveConfig();
    if (StateMachine_IsRun(sm)) StateMachine_RequestStandby(sm, now);
    /* GPIO20由10ms台架分支跟随GPIO21；1ms任务只持续封锁PWM。 */
#endif

    vdc_min = g_measurement.vdc_v[0];
    for (i = 1U; i < 6U; i++)
    {
        if (g_measurement.vdc_v[i] < vdc_min)
        {
            vdc_min = g_measurement.vdc_v[i];
        }
    }
    g_precharge_vdc_min = vdc_min;

    if (rs->seq_state != START_SEQ_IDLE)
    {
        g_start_seq_timer_ms = StartSeq_ElapsedMs(now, rs->seq_entry_tick);
    }

    if (StateMachine_IsFault(sm))
    {
        rs->restart_inhibit = 1U;
        PWM_BlockOutput();
        StartSeq_OpenPowerPath(rs);
        DrvGpio_WriteRunState(0U);
        return;
    }

    if (rs->seq_state == START_SEQ_PRECHARGE)
    {
#if (BOARD_LOW_VOLTAGE_DIRECT_TEST != 0U)
        /* 低压直测模式不应进入预充状态；若状态异常则立即回安全态。 */
        PWM_BlockOutput();
        StartSeq_OpenPowerPath(rs);
        rs->restart_inhibit = 1U;
        g_start_seq_fail = 1U;
        StateMachine_RequestStandby(sm, now);
        DrvGpio_WriteRunState(0U);
#else
        if (vdc_min >= g_precharge_done_v)
        {
            StartSeq_WriteBypass(1U);
            rs->seq_state = START_SEQ_BYPASS_WAIT;
            rs->seq_entry_tick = now;
            g_start_seq_state = (uint16_t)START_SEQ_BYPASS_WAIT;
            g_start_seq_timer_ms = 0UL;
        }
        else if (StartSeq_ElapsedMs(now, rs->seq_entry_tick)
                 >= g_precharge_timeout_ms)
        {
            PWM_BlockOutput();
            StartSeq_WriteBypass(0U);
            StartSeq_WriteGridSwitch(0U);
            ClosedLoop_ClearActiveConfig();
            StartSeq_Reset(rs);
            rs->restart_inhibit = 1U;
            g_start_seq_fail = 1U;
            StateMachine_RequestStandby(sm, now);
            DrvGpio_WriteRunState(0U);
        }
#endif
    }
}
