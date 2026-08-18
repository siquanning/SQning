/* Created by Siquanning */
#include <math.h>
#include "DSP2833x_Device.h"
#include "firmware/drivers/drv_timer.h"
#include "firmware/drivers/drv_sci.h"
#include "firmware/drivers/drv_adc.h"
#include "firmware/drivers/drv_epwm.h"
#include "firmware/drivers/drv_gpio.h"
#include "firmware/drivers/drv_interrupt.h"
#include "firmware/bsp/board.h"
#include "firmware/bsp/board_config.h"
#include "firmware/control/control_faststep.h"
#include "firmware/control/control_openloop.h"
#include "firmware/control/control_pll.h"
#include "firmware/control/control_global.h"
#include "firmware/control/control_closedloop.h"
#include "firmware/services/measurement.h"
#include "firmware/app/debug_snapshot.h"
#include "firmware/app/isr.h"
#include "firmware/app/run_supervisor.h"
#include "firmware/app/diagnostics.h"

/* ---- PLL 软切换 float 辅助 ---- */
#define PLL_SW_TWO_PI      6.283185307f
#define PLL_SW_PI          3.141592654f
#define PLL_SW_PI_HALF     1.570796327f
#define PLL_SW_TWO_PI_3    2.094395102f
#define PLL_SW_RAD2DEG     57.29577951f

/*
 * 固定时间复杂度 wrap (无 while 循环)。
 * 输入有界性保证: wrap_pi 的 |x| < 3π (φ_pll∈[π/2,5π/2) 与 φ_lut∈[0,2π)
 * 之差最大 2.5π), wrap_2pi 的 x ∈ (−2π, 4π) —
 * 由相位差在端点 (α=0/α=1) 每拍 rebase 到 ±π 保证,
 * 输入不会无界累积整圈, 单次条件修正即可归位。
 */
static float isr_wrap_pi(float x)
{
    if (x >  PLL_SW_PI) x -= PLL_SW_TWO_PI;
    if (x < -PLL_SW_PI) x += PLL_SW_TWO_PI;
    return x;
}

static float isr_wrap_2pi(float x)
{
    if (x >= PLL_SW_TWO_PI) x -= PLL_SW_TWO_PI;
    if (x < 0.0f)           x += PLL_SW_TWO_PI;
    return x;
}

static SciRxQueue    *g_pSciRxQueue   = ((SciRxQueue *)0);
static ControlContext *g_pControl      = ((ControlContext *)0);
static StateMachine  *g_pStateMachine = ((StateMachine *)0);

volatile uint16_t g_vdc_raw[6];
volatile uint16_t g_vac_raw[3];
volatile uint16_t g_iac_raw[3];
volatile uint32_t g_adc_frame_count;

volatile uint32_t g_scic_rx_isr_count = 0U;
volatile uint16_t g_scic_rx_last_byte = 0U;

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
    (void)pm;
}

void App_IsrSetTelemetry(Telemetry *t)
{
    (void)t;
}

__interrupt void App_Timer0Isr(void)
{
    uint32_t t0 = Diagnostics_CycleRead();

    DrvTimer0_OnInterrupt();
    DrvInterrupt_AckGroup1();

#if (BOARD_CLOCK_BRINGUP_ONLY != 0U)
    /*
     * Clock Bring-up 测试输出：翻转 GPIO67（LED_TX，board_pins.h 定义）。
     * Timer0=100us → 每 ISR 翻转 → 5kHz 方波（周期 200us）。
     * GPIO67 仅连板载 LED，安全；用于示波器验证 100us Timer 链路。
     */
    DrvGpio_Toggle(67U);   /* = BOARD_PIN_LED_TX */
#endif

    Diagnostics_WcetUpdate(&Diagnostics_Get()->timer0_isr,
                           t0 - Diagnostics_CycleRead());
}

__interrupt void App_ScicRxIsr(void)
{
    uint32_t t0 = Diagnostics_CycleRead();
    uint32_t now = Timebase_Now();
    uint16_t errFlags;
    int      hadError = 0;

    g_scic_rx_isr_count++;

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
            g_scic_rx_last_byte = b;
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
 * ADCRESULT6→Vab, ADCRESULT7→Vbc, ADCRESULT8→Vca,
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

/* ==================================================================
 * DebugSnapshot — 统一调试观测快照（见 debug_snapshot.h）
 * 在 20kHz 控制计算全部完成后更新一次；1ms JustFloat 前台只读。
 * 纯观测层：只换算/拷贝/派生观测值，不写任何控制或安全寄存器。
 * ================================================================== */
DebugSnapshot g_dbg_snap;

static void DebugSnapshot_Update(void)
{
    static const float iac_polarity[3] = {
        BOARD_IAC_IA_POLARITY, BOARD_IAC_IB_POLARITY, BOARD_IAC_IC_POLARITY };
    const uint16_t iac_offset[3] = { g_iac_ia_offset_counts,
        g_iac_ib_offset_counts, g_iac_ic_offset_counts };
    const uint16_t vdc_offset[6] = { g_vdc1_offset_counts, g_vdc2_offset_counts,
        g_vdc3_offset_counts, g_vdc4_offset_counts,
        g_vdc5_offset_counts, g_vdc6_offset_counts };
    DebugSnapshot *s = &g_dbg_snap;
    uint16_t i;

    /*
     * 轻量六通道：vline/vac 复用本拍 PLL 输入；
     * iac 自行换算（闭环块内 iac[] 仅 RUN 期间存在）；
     * vdc 用与控制相同的 offset/换算；pll_vmag/theta 供 mode 2 跟随波。
     */
    for (i = 0U; i < 3U; i++) {
        s->vac[i] = g_pll_input_vabc[i];
        s->vline[i] = g_pll_input_vline[i];
        s->iac[i] = Measurement_ConvertIac(g_iac_raw[i], iac_offset[i],
                                           iac_polarity[i]);
    }
    for (i = 0U; i < 6U; i++) {
        s->vdc[i] = Measurement_ConvertVdc(g_vdc_raw[i], vdc_offset[i]);
    }
    s->pll_vmag = g_pll.vmag;
    s->pll_theta = g_pll.theta;
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

    /*
     * PLL 始终运行: ADC 线电压 → 重构相电压 → SRF-PLL → 更新 g_pll。
     * 锁定后前台判决置 g_pll_switch_req=1, 下方切换块经 alpha 淡化
     * 将调制参考相位从开环 LUT 平滑切换至 PLL 同步相位。
     */
    {
#if (BOARD_PLL_INPUT_SIMULATION != 0U)
        /*
         * 50 Hz bench source at 20 kHz.  Do not call cosf() three times in
         * this ISR: those library calls can starve the foreground 1 ms
         * telemetry task on the C28x.  Rotate one unit vector instead and
         * derive all three phases algebraically.
         */
        static float s_pll_sim_cos = 1.0f;
        static float s_pll_sim_sin = 0.0f;
        static uint16_t s_pll_sim_norm_div = 0U;
        const float step_cos = 0.999876632f;  /* cos(2*pi*50/20000) */
        const float step_sin = 0.015707317f;  /* sin(2*pi*50/20000) */
        float next_cos;
        float next_sin;
        float va, vb, vc;

        next_cos = s_pll_sim_cos * step_cos - s_pll_sim_sin * step_sin;
        next_sin = s_pll_sim_sin * step_cos + s_pll_sim_cos * step_sin;
        s_pll_sim_cos = next_cos;
        s_pll_sim_sin = next_sin;

        /* Cheap periodic unit-length correction; no sqrtf in the ISR. */
        if (++s_pll_sim_norm_div >= 256U) {
            float norm_gain = 0.5f * (3.0f
                            - s_pll_sim_cos * s_pll_sim_cos
                            - s_pll_sim_sin * s_pll_sim_sin);
            s_pll_sim_cos *= norm_gain;
            s_pll_sim_sin *= norm_gain;
            s_pll_sim_norm_div = 0U;
        }

        va = BOARD_PLL_SIM_VPEAK_V * s_pll_sim_cos;
        vb = BOARD_PLL_SIM_VPEAK_V
           * (-0.5f * s_pll_sim_cos + 0.866025404f * s_pll_sim_sin);
        vc = BOARD_PLL_SIM_VPEAK_V
           * (-0.5f * s_pll_sim_cos - 0.866025404f * s_pll_sim_sin);
        g_pll_input_vline[0] = va - vb;
        g_pll_input_vline[1] = vb - vc;
        g_pll_input_vline[2] = vc - va;
#else
        float vab, vbc, vca, va, vb, vc;
        vab = Measurement_ConvertVac(g_vac_raw[0],
                        g_vac_vab_offset_counts, BOARD_VAC_VAB_POLARITY);
        vbc = Measurement_ConvertVac(g_vac_raw[1],
                        g_vac_vbc_offset_counts, BOARD_VAC_VBC_POLARITY);
        vca = Measurement_ConvertVac(g_vac_raw[2],
                        g_vac_vca_offset_counts, BOARD_VAC_VCA_POLARITY);
        Measurement_LineToPhase(vab, vbc, vca, &va, &vb, &vc);
        g_pll_input_vline[0] = vab;
        g_pll_input_vline[1] = vbc;
        g_pll_input_vline[2] = vca;
#endif
        g_pll_input_vabc[0] = va;
        g_pll_input_vabc[1] = vb;
        g_pll_input_vabc[2] = vc;
        PLL_Run(&g_pll, va, vb, vc, BOARD_CONTROL_TS);
    }

    /*
     * 50 Hz 调制参考 — 开环 LUT 与 PLL 同步的相位交叉淡化切换。
     *
     * 相位约定: φLUT = 2π·idx/1200 (idx=0 → A 相正过零)
     *           φPLL = θ + π/2     (θ=0 → A 相正峰值)
     * 输出统一为 M·sin(φout), φout = φLUT + α·Δ。
     * Δ 为连续相位差: 端点每拍 rebase 到 ±π, 淡化期间增量式
     * unwrap (跨 ±π 不跳变)。
     * LUT 每拍都运行, phase_index 保持热更新 → 回退时相位连续。
     */
    {
        static float s_alpha = 0.0f;             /* 0=纯开环, 1=纯PLL */
        static float s_err_unwrapped = 0.0f;     /* 相位差 [rad], 端点 rebase 到 ±π, 仅淡化期间增长 */
        const float fade_step = BOARD_CONTROL_TS
                              / (BOARD_PLL_FADE_MS * 0.001f);
        float phi_lut, phi_pll, phi_out;
        float e_cur, diff;
        uint16_t idx;

        idx = OpenLoop_GetPhaseIndex();
        OpenLoop_GenerateSine(mabc);

        /*
         * 相位差跟踪:
         * 端点 (α=0 纯开环 / α=1 纯PLL) 每拍 rebase 到最短等价相位差
         * (±π 内), 防止长期小频差下整圈无界累积 (也保证 wrap 输入有界);
         * 淡化期间 (0<α<1) 增量式 unwrap, 跨 ±π branch 不跳变。
         * α 用本拍斜坡更新前的值判断: 端点拍 rebase, 进入淡化拍起 unwrap,
         * 边界两拍相位差仅差一拍拍频漂移 (≤0.1°), 输出连续。
         */
        phi_lut = (float)idx * PLL_SW_TWO_PI / (float)OPENLOOP_LUT_SIZE;
        phi_pll = g_pll.theta + PLL_SW_PI_HALF;
        e_cur   = isr_wrap_pi(phi_pll - phi_lut);

        if ((s_alpha <= 0.0f) || (s_alpha >= 1.0f)) {
            s_err_unwrapped = e_cur;
        } else {
            diff = isr_wrap_pi(e_cur - isr_wrap_pi(s_err_unwrapped));
            s_err_unwrapped += diff;
        }

        /* α 双向斜坡 (req 由前台 10ms 锁定判决维护) */
        if (g_pll_switch_req != 0U) {
            if (s_alpha < 1.0f) {
                s_alpha += fade_step;
                if (s_alpha > 1.0f) s_alpha = 1.0f;
            }
        } else {
            if (s_alpha > 0.0f) {
                s_alpha -= fade_step;
                if (s_alpha < 0.0f) s_alpha = 0.0f;
            }
        }
        g_switch_alpha = s_alpha;

        /* α>0 时 float 路径覆盖 mabc (α=0 保持纯 int LUT 输出) */
        phi_out = phi_lut;
        if (s_alpha > 0.0f) {
            phi_out = isr_wrap_2pi(phi_lut + s_alpha * s_err_unwrapped);
            mabc[0] = (int16_t)((float)BOARD_PLL_SW_MOD_PERMILL
                      * sinf(phi_out));
            mabc[1] = (int16_t)((float)BOARD_PLL_SW_MOD_PERMILL
                      * sinf(phi_out - PLL_SW_TWO_PI_3));
            mabc[2] = (int16_t)((float)BOARD_PLL_SW_MOD_PERMILL
                      * sinf(phi_out + PLL_SW_TWO_PI_3));
        }

        g_switch_phase_err_deg =
            isr_wrap_pi(phi_pll - phi_out) * PLL_SW_RAD2DEG;
    }

#if (BOARD_PLL_RELAY_TEST_ONLY != 0U)
    /* 台架模式不执行任何闭环采样换算或PI快步，仅保留PLL路径。 */
    mabc[0] = 0; mabc[1] = 0; mabc[2] = 0;
#else
    {
        static uint16_t s_closedloop_was_enabled = 0U;
        static const float iac_polarity[3] = {
            BOARD_IAC_IA_POLARITY, BOARD_IAC_IB_POLARITY, BOARD_IAC_IC_POLARITY};
        const uint16_t iac_offset[3] = {g_iac_ia_offset_counts,
            g_iac_ib_offset_counts, g_iac_ic_offset_counts};
        const uint16_t vdc_offset[6] = {g_vdc1_offset_counts, g_vdc2_offset_counts,
            g_vdc3_offset_counts, g_vdc4_offset_counts,
            g_vdc5_offset_counts, g_vdc6_offset_counts};
        float vac[3], iac[3], vdc[6];
        uint16_t enable, is_run, valid, i;
        uint16_t mode = ClosedLoop_GetActiveRunMode();
        is_run = ((g_pStateMachine != ((StateMachine *)0)) &&
                  StateMachine_IsRun(g_pStateMachine)) ? 1U : 0U;
        enable = ((is_run != 0U) &&
                  (ClosedLoop_IsValidRunMode(mode) != 0U) &&
                  (g_pll_switch_req != 0U) &&
                  (g_switch_alpha >= g_pll_ready_alpha_min)) ? 1U : 0U;

        /*
         * STANDBY/FAULT下ClosedLoop_Init和1ms慢环已保持状态清零，无需在
         * 每个20kHz中断重复9路浮点换算及三相reset_phase。退出RUN后的
         * 第一拍仍完整调用一次disable路径，确保积分器/QSG立即清零。
         * RUN期间（包括PLL暂未就绪）保留原有输入有限性校验和故障路径。
         */
        valid = 1U;
        if ((is_run != 0U) || (s_closedloop_was_enabled != 0U)) {
            for (i = 0U; i < 3U; i++) {
                vac[i] = g_pll_input_vabc[i];
                iac[i] = Measurement_ConvertIac(g_iac_raw[i], iac_offset[i], iac_polarity[i]);
            }
            for (i = 0U; i < 6U; i++)
                vdc[i] = Measurement_ConvertVdc(g_vdc_raw[i], vdc_offset[i]);
            valid = ClosedLoop_FastStepAll(enable, vac, iac, g_pll.theta,
                                           g_pll.freq * PLL_SW_TWO_PI,
                                           vdc, BOARD_CONTROL_TS, mabc);
        } else {
            mabc[0] = 0; mabc[1] = 0; mabc[2] = 0;
        }
        s_closedloop_was_enabled = enable;

        if ((valid == 0U) && (is_run != 0U)) {
            PWM_BlockOutput();
            ClosedLoop_ClearActiveConfig();
            System_EnterFault(g_pStateMachine, FAULT_SW_CONTROL_INVALID,
                              Timebase_Now());
            mabc[0] = 0; mabc[1] = 0; mabc[2] = 0;
        }
    }
#endif

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

    /*
     * Write UNI polarity immediately after modulation shadows,
     * using the NEW mabc.  GPIO27/28/29 settle within this ISR,
     * well before the next CTR=ZERO loads the new PWM values.
     */
    {
        static uint16_t s_debug_snapshot_div = 0U;
        uint16_t uni_a = (mabc[0] >= 0) ? 1U : 0U;
        uint16_t uni_b = (mabc[1] >= 0) ? 1U : 0U;
        uint16_t uni_c = (mabc[2] >= 0) ? 1U : 0U;

        DrvGpio_WriteUniPolarity(uni_a, uni_b, uni_c);

        /* 快照是纯观测层：按JustFloat的1kHz消费速率更新，控制仍为20kHz。 */
        if (s_debug_snapshot_div == 0U) {
            DebugSnapshot_Update();
        }
        if (++s_debug_snapshot_div >= (uint16_t)BOARD_DEBUG_SNAPSHOT_DIVIDER)
            s_debug_snapshot_div = 0U;
    }

    DrvEpwm_ClearIntFlag(BOARD_EPWM_MODULE);
    DrvInterrupt_AckGroup3();

    Diagnostics_WcetUpdate(&diag->fast_isr,
                           t0 - Diagnostics_CycleRead());
}

/*
 * A/B/C共用Trip Zone ISR — ePWM1/ePWM3/ePWM5任一代表模块的TZ中断。
 *
 * Safety contract:
 *   1. PWM_BlockOutput先拉低GPIO30，再关闭全部OST中断并强制六路OST。
 *   2. OST锁存绝不在ISR中清除，保持到受控的下一次启动。
 *   3. System_EnterFault锁存统一FAULT_HW_TZ_TRIP；后台沿原流程断GPIO23/22。
 *
 * TZEINT.OST is re-armed by DrvEpwm_ClearOstTrip() during recovery.
 */
__interrupt void App_EpwmTzIsr(void)
{
    /*
     * 能进入此ISR本身就表示某个已使能代表模块产生TZ中断。非测试相的
     * TZEINT.OST保持关闭，因此不会由软件维持的OST锁存误触发这里。
     */
    PWM_BlockOutput();
    ClosedLoop_ClearActiveConfig();
    if (g_pStateMachine != ((StateMachine *)0))
    {
        System_EnterFault(g_pStateMachine,
                          FAULT_HW_TZ_TRIP, Timebase_Now());
    }

    DrvInterrupt_AckGroup2();
}
