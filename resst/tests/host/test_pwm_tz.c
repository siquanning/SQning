/*
 * PWM TZ 寄存器级 host 测试 — 编译真实 drv_epwm.c, 验证:
 *   - DisableOstInt 关闭 OST interrupt + 清 INT 标志
 *   - 封锁序列 (先全部 Disable, 再全部 Force): 软件封锁不触发 TZ 中断
 *   - 释放序列 (ClearOstTrip): 清锁存 + 重新武装 OST interrupt
 *   - RUN 中真实 TZ 跳闸: armed 状态触发中断 (→ TZ ISR → FAULT)
 *   - 多次启停循环: 重新武装持续有效, 第二次 RUN 中真实 TZ 仍触发
 *
 * 封锁/释放的两遍循环顺序与 firmware/bsp/board.c 中
 * PWM_BlockOutput/PWM_ReleaseOutput 完全一致 (board.c 为 target-only,
 * host 无法直接编译, 由本测试按相同顺序调用公开原语验证语义)。
 *
 * TZ 锁存/中断触发的硬件行为由测试模型模拟 (fake 寄存器为纯存储):
 *   - 跳闸 (软件 TZFRC.OST 或真实 TZ1/TZ2 引脚) → TZFLG.OST 锁存;
 *     当时 TZEINT.OST==1 则触发 TZ interrupt (真实硬件上会进 TZ ISR)。
 *   - TZCLR.OST=1 → 锁存清除。
 */

#include <stdio.h>
#include <string.h>
#include "DSP2833x_Device.h"
#include "firmware/drivers/drv_epwm.h"

/* ---- 寄存器实例 ---- */
volatile struct EPWM_REGS    EPwm1Regs;
volatile struct EPWM_REGS    EPwm2Regs;
volatile struct EPWM_REGS    EPwm3Regs;
volatile struct EPWM_REGS    EPwm4Regs;
volatile struct EPWM_REGS    EPwm5Regs;
volatile struct EPWM_REGS    EPwm6Regs;
volatile struct SYS_CTRL_REGS  SysCtrlRegs;
volatile struct GPIO_CTRL_REGS GpioCtrlRegs;

/* ---- TZ 硬件行为模型 ---- */
static int g_latched[7];   /* TZFLG.OST 锁存模型, 下标=模块号 */
static int g_int_fired[7]; /* OST interrupt 触发次数 */

static int g_failures = 0;
static int g_unified_fault_count;

#define FAIL(msg) do { \
    printf("FAIL: %s\n", (msg)); fflush(stdout); \
    g_failures++; \
} while(0)

static volatile struct EPWM_REGS *reg(int m)
{
    switch (m)
    {
    case 1: return &EPwm1Regs;
    case 2: return &EPwm2Regs;
    case 3: return &EPwm3Regs;
    case 4: return &EPwm4Regs;
    case 5: return &EPwm5Regs;
    case 6: return &EPwm6Regs;
    default: return ((volatile struct EPWM_REGS *)0);
    }
}

/* 跳闸 (软件 force 或真实 TZ 引脚): 锁存 + armed 时触发中断 */
static void model_trip(int m)
{
    g_latched[m] = 1;
    if (reg(m)->TZEINT.bit.OST != 0U)
    {
        g_int_fired[m]++;
    }
}

/* TZCLR.OST=1 的硬件效果: 锁存清除 */
static void model_clear_ost(int m)
{
    g_latched[m] = 0;
}

static void reset_model(void)
{
    int m;
    memset((void *)&EPwm1Regs, 0, sizeof(EPwm1Regs));
    memset((void *)&EPwm2Regs, 0, sizeof(EPwm2Regs));
    memset((void *)&EPwm3Regs, 0, sizeof(EPwm3Regs));
    memset((void *)&EPwm4Regs, 0, sizeof(EPwm4Regs));
    memset((void *)&EPwm5Regs, 0, sizeof(EPwm5Regs));
    memset((void *)&EPwm6Regs, 0, sizeof(EPwm6Regs));
    memset((void *)&SysCtrlRegs, 0, sizeof(SysCtrlRegs));
    memset((void *)&GpioCtrlRegs, 0, sizeof(GpioCtrlRegs));
    for (m = 0; m <= 6; m++)
    {
        g_latched[m]  = 0;
        g_int_fired[m] = 0;
    }
    g_unified_fault_count = 0;
}

static void test_common_tz_configuration(void)
{
    DrvEpwmConfig cfg;
    int m;

    reset_model();
    cfg.tbclk_hz = 140000000UL;
    cfg.pwm_freq_hz = 20000UL;
    cfg.count_mode = 2U;
    cfg.db_red = 140U;
    cfg.db_fed = 140U;
    cfg.tz_sources = 0x0300U; /* OSHT1 + OSHT2 */
    cfg.tz_oneshot_enable = 0U;
    cfg.tz_cbc_enable = 0U;

    for (m = 1; m <= 6; m++) {
        if (DrvEpwm_Init((uint32_t)m, &cfg) != 0)
            FAIL("T0.1: all six ePWM modules initialize");
    }
    for (m = 1; m <= 6; m++) {
        if (reg(m)->TZSEL.all != 0x0300U)
            FAIL("T0.2: all TZSEL equal OSHT1+OSHT2");
        if (reg(m)->TZSEL.all != reg(1)->TZSEL.all)
            FAIL("T0.3: ePWM1-6 TZSEL must be identical");
        if (reg(m)->TZCTL.bit.TZA != 2U || reg(m)->TZCTL.bit.TZB != 2U)
            FAIL("T0.4: TZ action forces both A/B outputs LOW");
    }

    DrvEpwm_ConfigGpio(1U);
    if (GpioCtrlRegs.GPAMUX1.bit.GPIO12 != 1U ||
        GpioCtrlRegs.GPAMUX1.bit.GPIO13 != 1U)
        FAIL("T0.5: GPIO12/13 mux to shared TZ1/TZ2 inputs");
}

/* 与 board.c PWM_BlockOutput 相同的两遍循环: 先全部 Disable, 再全部 Force */
static void model_block_output(void)
{
    int m;
    for (m = 1; m <= 6; m++) DrvEpwm_DisableOstInt((uint32_t)m);
    for (m = 1; m <= 6; m++) { DrvEpwm_ForceTrip((uint32_t)m); model_trip(m); }
}

/* 与 board.c PWM_ReleaseOutput 相同顺序: 每模块 Clear OST/INT + Re-arm */
static void model_release_output(void)
{
    int m;
    for (m = 1; m <= 6; m++) { DrvEpwm_ClearOstTrip((uint32_t)m); model_clear_ost(m); }
}

/* 与board.c PWM_ReleaseSelectedPhase一致：只清选中相两模块，其余保持OST。 */
static void model_release_selected_phase(int phase)
{
    int m;
    int first = (phase - 1) * 2 + 1;
    for (m = 1; m <= 6; m++)
    {
        if (m == first || m == first + 1) {
            DrvEpwm_ClearOstTrip((uint32_t)m);
            model_clear_ost(m);
        } else {
            DrvEpwm_DisableOstInt((uint32_t)m);
            DrvEpwm_ForceTrip((uint32_t)m);
            model_trip(m);
        }
    }
}

/* ==================================================================
 * T1: DisableOstInt — OST interrupt 关闭 + 执行 TZCLR.INT 清标志写
 * (TZCLR 为写1清0寄存器: fake 纯存储读回 1 = 代码确实执行了该写)
 * ================================================================== */
static void test_disable_ost_int(void)
{
    int m;
    reset_model();
    for (m = 1; m <= 6; m++)
    {
        reg(m)->TZEINT.bit.OST = 1U;
        reg(m)->TZCLR.bit.INT  = 0U;
        DrvEpwm_DisableOstInt((uint32_t)m);
        if (reg(m)->TZEINT.bit.OST != 0U)
            FAIL("T1.1: DisableOstInt 后 TZEINT.OST 应为 0");
        if (reg(m)->TZCLR.bit.INT != 1U)
            FAIL("T1.2: DisableOstInt 应执行 TZCLR.INT 清标志写");
    }
}

/* ==================================================================
 * T2: 封锁序列 (① 核心) — armed 状态下 STOP, 软件封锁不触发中断
 * ================================================================== */
static void test_block_sequence(void)
{
    int m;
    reset_model();
    for (m = 1; m <= 6; m++) reg(m)->TZEINT.bit.OST = 1U;  /* RUN 状态: armed */

    model_block_output();

    for (m = 1; m <= 6; m++)
    {
        if (reg(m)->TZEINT.bit.OST != 0U)
            FAIL("T2.1: 封锁后全部模块 OST interrupt 关闭");
        if (g_latched[m] != 1)
            FAIL("T2.2: OST 锁存生效 (输出被 TZ_FORCE_LO 封锁)");
        if (g_int_fired[m] != 0)
            FAIL("T2.3: 软件封锁不触发 TZ 中断 (STOP 不误进 FAULT)");
    }
}

/* ==================================================================
 * T3: 释放序列 — ClearOstTrip 清锁存 + 重新武装 OST interrupt
 * ================================================================== */
static void test_release_sequence(void)
{
    int m;
    reset_model();
    model_block_output();   /* 先封锁 (armed → disarmed, 锁存置位) */

    model_release_output();

    for (m = 1; m <= 6; m++)
    {
        if (g_latched[m] != 0)
            FAIL("T3.1: ClearOstTrip 后 OST 锁存清除");
        if (reg(m)->TZEINT.bit.OST != 1U)
            FAIL("T3.2: ClearOstTrip 后 OST interrupt 重新武装");
    }
}

/* ==================================================================
 * T4: (③ 核心) RUN 中真实 TZ1/TZ2 跳闸 — armed 状态触发中断 → FAULT
 * ================================================================== */
static void test_real_trip_in_run(void)
{
    reset_model();
    reg(1)->TZEINT.bit.OST = 1U;   /* RUN 状态: armed */

    model_trip(1);                 /* 真实 TZ 引脚有效 */

    if (g_latched[1] != 1)
        FAIL("T4.1: 真实跳闸锁存 OST");
    if (g_int_fired[1] != 1)
        FAIL("T4.2: armed 状态真实跳闸触发 TZ 中断 (进 FAULT_HW_TZ_TRIP)");
}

/* ==================================================================
 * T5: (⑤ 核心) 多次启停循环 — 重新武装持续有效,
 *     第二次 RUN 中真实 TZ 仍能触发 FAULT
 * ================================================================== */
static void test_restart_rearm(void)
{
    reset_model();

    /* 首次启动: release → armed */
    model_release_output();

    /* 停止: block → disarm + 封锁, 不触发中断 */
    model_block_output();
    if (g_int_fired[1] != 0)
        FAIL("T5.1: 停止不触发 TZ 中断");

    /* 再次启动: release → 重新武装 */
    model_release_output();
    if (reg(1)->TZEINT.bit.OST != 1U)
        FAIL("T5.2: STOP→START 后 OST interrupt 重新武装");
    if (g_latched[1] != 0)
        FAIL("T5.3: 重新启动后锁存已清除");

    /* 第二次 RUN 中真实 TZ 跳闸 */
    model_trip(1);
    if (g_int_fired[1] != 1)
        FAIL("T5.4: 第二次真实跳闸仍触发中断 (重启后保护依然有效)");

    /* 再完整循环一次 */
    model_block_output();
    model_release_output();
    model_trip(1);
    if (g_int_fired[1] != 2)
        FAIL("T5.5: 多次启停循环保护持续有效");
}

/* ==================================================================
 * T6: 无效模块守卫
 * ================================================================== */
static void test_invalid_module(void)
{
    reset_model();
    DrvEpwm_ForceTrip(0U);
    DrvEpwm_ForceTrip(7U);
    DrvEpwm_ClearOstTrip(0U);
    DrvEpwm_ClearOstTrip(7U);
    DrvEpwm_DisableOstInt(7U);
    if (DrvEpwm_GetTripStatus(7U) != 0U)
        FAIL("T6.1: 无效模块 GetTripStatus 返回 0");
    if (DrvEpwm_GetPeriod(0U) != 0U)
        FAIL("T6.2: 无效模块 GetPeriod 返回 0");
    /* 走到这里即通过 */
}

static void test_selected_phase_release(void)
{
    int phase, m;
    for (phase = 1; phase <= 3; phase++)
    {
        int first = (phase - 1) * 2 + 1;
        reset_model();
        model_block_output();
        model_release_selected_phase(phase);
        for (m = 1; m <= 6; m++)
        {
            if (m == first || m == first + 1) {
                if (g_latched[m] != 0 || reg(m)->TZEINT.bit.OST == 0U)
                    FAIL("T7.1: selected phase must clear OST and re-arm");
            } else {
                if (g_latched[m] == 0 || reg(m)->TZEINT.bit.OST != 0U)
                    FAIL("T7.2: non-selected phases must remain OST blocked");
            }
        }
    }
}

/* 模拟生产ISR的统一入口：GPIO30关闭/六路OST在模型中表现为全局block。 */
static void model_unified_tz_isr(void)
{
    model_block_output();
    g_unified_fault_count++;
}

static void test_three_phase_unified_fault_path(void)
{
    int phase;
    for (phase = 1; phase <= 3; phase++) {
        int representative = (phase - 1) * 2 + 1; /* ePWM1/ePWM3/ePWM5 */
        int m;
        reset_model();
        model_block_output();
        model_release_selected_phase(phase);

        /* 同相两模块TZSEL一致，共享TZ1/TZ2；代表模块必定同时跳闸。 */
        model_trip(representative);
        if (g_int_fired[representative] == 1)
            model_unified_tz_isr();

        if (g_unified_fault_count != 1)
            FAIL("T8.1: A/B/C representative TZ reaches unified FAULT entry");
        for (m = 1; m <= 6; m++) {
            if (g_latched[m] == 0 || reg(m)->TZEINT.bit.OST != 0U)
                FAIL("T8.2: unified TZ entry leaves all six modules OST blocked/disarmed");
        }
    }
}

static void test_all_six_released_then_any_phase_faults_globally(void)
{
    int phase;
    for (phase = 1; phase <= 3; phase++) {
        int representative = (phase - 1) * 2 + 1;
        int m;
        reset_model();
        model_block_output();
        model_release_output();
        for (m = 1; m <= 6; m++) {
            if (g_latched[m] != 0 || reg(m)->TZEINT.bit.OST == 0U)
                FAIL("T9.1: three-phase release clears and arms ePWM1-6");
        }
        model_trip(representative);
        if (g_int_fired[representative] == 1) model_unified_tz_isr();
        if (g_unified_fault_count != 1)
            FAIL("T9.2: any A/B/C trip reaches unified FAULT in three-phase mode");
        for (m = 1; m <= 6; m++) {
            if (g_latched[m] == 0 || reg(m)->TZEINT.bit.OST != 0U)
                FAIL("T9.3: any phase trip blocks all six PWM modules");
        }
    }
}

int main(void)
{
    printf("=== PWM TZ (OST interrupt 封锁/释放序列) Tests ===\n\n");

    test_common_tz_configuration();
    test_disable_ost_int();
    test_block_sequence();
    test_release_sequence();
    test_real_trip_in_run();
    test_restart_rearm();
    test_invalid_module();
    test_selected_phase_release();
    test_three_phase_unified_fault_path();
    test_all_six_released_then_any_phase_faults_globally();

    printf("\n=== %s ===\n", (g_failures == 0) ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return (g_failures > 0) ? 1 : 0;
}
