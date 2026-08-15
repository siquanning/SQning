#ifdef __TMS320C28XX__
static int _host_test_placeholder_run_control;
#else

#include <stdio.h>
#include <stdint.h>
#include "firmware/app/run_control.h"
#include "firmware/bsp/board_config.h"

static int g_failures = 0;

#define FAIL(msg) do { \
    printf("FAIL: %s\n", (msg)); fflush(stdout); \
    g_failures++; \
} while(0)

#define TICKS  BOARD_RUN_BTN_DEBOUNCE_TICKS   /* 50ms = 5 拍 */

/*
 * 连续喂 N 拍电平 (1=有效电平/按下保持, 0=松开保持), 无返回值语义 —
 * 保持型按钮: 电平本身即请求, 消抖后稳定电平由 GetStableLevel 读取。
 */
static void feed(RunControl *rc, uint16_t level, int ticks)
{
    int i;

    for (i = 0; i < ticks; i++)
    {
        RunControl_Sample(rc, level);
    }
}

/* ==================================================================
 * T1: 上电初始 → 稳定 0 (STOP 请求)
 * ================================================================== */
static void test_init_level_zero(void)
{
    RunControl rc;
    RunControl_Init(&rc);

    if (RunControl_GetStableLevel(&rc) != 0U)
        FAIL("T1.1: 上电初始稳定电平应为 0");
    if (rc.stable != 0U)
        FAIL("T1.2: stable 字段应为 0");
}

/* ==================================================================
 * T2: 保持型按下 — 连续 50ms 高 → 稳定 1, 继续按住保持 1
 * ================================================================== */
static void test_latch_on(void)
{
    RunControl rc;
    RunControl_Init(&rc);

    feed(&rc, 1U, TICKS - 1);              /* 前 4 拍: 未满 50ms */
    if (RunControl_GetStableLevel(&rc) != 0U)
        FAIL("T2.1: 按下 40ms 未满 50ms, 应仍为 0");

    feed(&rc, 1U, 1);                      /* 第 5 拍: 稳定 1 */
    if (RunControl_GetStableLevel(&rc) != 1U)
        FAIL("T2.2: 连续按下 50ms 后稳定电平应为 1");

    feed(&rc, 1U, 1000 - TICKS);           /* 保持按住 10s */
    if (RunControl_GetStableLevel(&rc) != 1U)
        FAIL("T2.3: 保持按住期间稳定电平应持续为 1");
    if (rc.cnt_hi != TICKS)
        FAIL("T2.4: 连续高计数应封顶在消抖拍数, 不回绕");
}

/* ==================================================================
 * T3: 保持型松开 — 连续 50ms 低 → 稳定 0 (对称消抖)
 * ================================================================== */
static void test_latch_off(void)
{
    RunControl rc;
    RunControl_Init(&rc);

    feed(&rc, 1U, TICKS);                  /* 先按下保持 → 稳定 1 */
    feed(&rc, 0U, TICKS - 1);              /* 松开 40ms */
    if (RunControl_GetStableLevel(&rc) != 1U)
        FAIL("T3.1: 松开 40ms 未满 50ms, 应仍为 1 (松开同样消抖)");

    feed(&rc, 0U, 1);                      /* 第 5 拍 */
    if (RunControl_GetStableLevel(&rc) != 0U)
        FAIL("T3.2: 连续松开 50ms 后稳定电平应为 0");
}

/* ==================================================================
 * T4: 短按 30ms (<50ms) → 电平不变
 * ================================================================== */
static void test_short_tap_no_change(void)
{
    RunControl rc;
    RunControl_Init(&rc);

    feed(&rc, 1U, 3);                      /* 按 30ms */
    feed(&rc, 0U, 3);                      /* 松 30ms */
    feed(&rc, 1U, 3);                      /* 再按 30ms */
    feed(&rc, 0U, 3);

    if (RunControl_GetStableLevel(&rc) != 0U)
        FAIL("T4.1: 全程 30ms 点按不应改变稳定电平");
}

/* ==================================================================
 * T5: 拨动过程抖动 → 不误切换
 * ================================================================== */
static void test_bounce_no_change(void)
{
    RunControl rc;
    RunControl_Init(&rc);

    feed(&rc, 1U, TICKS);                  /* 稳定 1 */
    if (RunControl_GetStableLevel(&rc) != 1U)
        FAIL("T5.1: 预置: 应为稳定 1");

    /* 松开过程抖动: 永远凑不齐连续 5 拍低 */
    feed(&rc, 0U, 2); feed(&rc, 1U, 1);
    feed(&rc, 0U, 3); feed(&rc, 1U, 1);
    feed(&rc, 0U, 4); feed(&rc, 1U, 1);

    if (RunControl_GetStableLevel(&rc) != 1U)
        FAIL("T5.2: 抖动过程稳定电平不应改变");
}

/* ==================================================================
 * T6: 完整启停循环 — 电平跟随保持型开关, 无 toggle 语义
 * ================================================================== */
static void test_full_cycle(void)
{
    RunControl rc;
    RunControl_Init(&rc);

    feed(&rc, 0U, TICKS);                  /* 上电松开 → 稳定 0 */
    if (RunControl_GetStableLevel(&rc) != 0U)
        FAIL("T6.1: 上电松开应稳定 0");

    feed(&rc, 1U, TICKS);                  /* 第一次按下保持 */
    if (RunControl_GetStableLevel(&rc) != 1U)
        FAIL("T6.2: 第一次按下后应稳定 1 (RUN 请求)");

    feed(&rc, 1U, 50);                     /* 按住任意久: 保持 1 */
    if (RunControl_GetStableLevel(&rc) != 1U)
        FAIL("T6.3: 按住期间应保持 1, 无自动翻转");

    feed(&rc, 0U, TICKS);                  /* 再按一次松开 */
    if (RunControl_GetStableLevel(&rc) != 0U)
        FAIL("T6.4: 再按一次后应稳定 0 (STOP 请求)");

    feed(&rc, 1U, TICKS);                  /* 第三次按下 */
    if (RunControl_GetStableLevel(&rc) != 1U)
        FAIL("T6.5: 第三次按下后应稳定 1");
}

/* ==================================================================
 * T7: NULL 守卫
 * ================================================================== */
static void test_null_guards(void)
{
    RunControl_Init(((RunControl *)0));
    RunControl_Sample(((RunControl *)0), 1U);
    if (RunControl_GetStableLevel(((const RunControl *)0)) != 0U)
        FAIL("T7.1: NULL GetStableLevel 应返回 0 (安全侧)");
}

int main(void)
{
    printf("=== Run Control (GPIO21 保持型按钮消抖电平) Tests ===\n");
    printf("Debounce ticks = %u (50ms @ 10ms)\n\n", (unsigned)TICKS);

    test_init_level_zero();
    test_latch_on();
    test_latch_off();
    test_short_tap_no_change();
    test_bounce_no_change();
    test_full_cycle();
    test_null_guards();

    printf("\n=== %s ===\n", (g_failures == 0) ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return (g_failures > 0) ? 1 : 0;
}

#endif /* !__TMS320C28XX__ */
