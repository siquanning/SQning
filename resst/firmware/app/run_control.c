#include "firmware/app/run_control.h"
#include "firmware/bsp/board_config.h"

void RunControl_Init(RunControl *rc)
{
    if (rc == ((RunControl *)0)) return;

    rc->stable = 0U;   /* STOP 请求 */
    rc->cnt_hi = 0U;
    rc->cnt_lo = 0U;
}

void RunControl_Sample(RunControl *rc, uint16_t active)
{
    if (rc == ((RunControl *)0)) return;

    /*
     * 对称消抖: 两个计数器互斥累加, 采样到相反电平即清零。
     * 封顶在 DEBOUNCE_TICKS — 保持型按钮任意久按住都不会回绕。
     */
    if (active != 0U)
    {
        if (rc->cnt_hi < BOARD_RUN_BTN_DEBOUNCE_TICKS) rc->cnt_hi++;
        rc->cnt_lo = 0U;
    }
    else
    {
        if (rc->cnt_lo < BOARD_RUN_BTN_DEBOUNCE_TICKS) rc->cnt_lo++;
        rc->cnt_hi = 0U;
    }

    /* 稳定电平直接跟随消抖结果 — 无沿事件, 无软件翻转 */
    if (rc->cnt_hi >= BOARD_RUN_BTN_DEBOUNCE_TICKS)
    {
        rc->stable = 1U;
    }
    else if (rc->cnt_lo >= BOARD_RUN_BTN_DEBOUNCE_TICKS)
    {
        rc->stable = 0U;
    }
}

uint16_t RunControl_GetStableLevel(const RunControl *rc)
{
    if (rc == ((const RunControl *)0)) return 0U;
    return rc->stable;
}
