#include "firmware/control/control_pll.h"
#include "firmware/bsp/board_config.h"
#include <math.h>

/*
 * PLL 参数 — 来自 CHB 级联 H 桥整流器原始设计。
 * 原设计在311V峰值下使用vq×0.001。低压测试改为(vq/vmag)×0.311，
 * 保持原额定点环路增益，同时避免锁相动态随输入幅值降低而显著变慢。
 */
#define PLL_OMEGA_NOM    314.159265f   /* 2π × 50 Hz */
#define PLL_KP_VQ        120.0f
#define PLL_KI_VQ        2000.0f
#define PLL_VQ_PU_SCALE   0.311f       /* (vq/vmag) → 原311V额定点尺度 */
#define PLL_INTEGRAL_MAX  31.416f      /* ±5 Hz in rad/s */
#define PLL_FREQ_MIN     282.7433f     /* 2π × 45 Hz */
#define PLL_FREQ_MAX     345.5752f     /* 2π × 55 Hz */
#define PLL_TWO_PI         6.283185307f

static float wrap_2pi(float x)
{
    while (x >= PLL_TWO_PI) x -= PLL_TWO_PI;
    while (x < 0.0f)       x += PLL_TWO_PI;
    return x;
}

static float clamp(float x, float lo, float hi)
{
    return (x < lo) ? lo : ((x > hi) ? hi : x);
}

/*
 * Clarke 变换: abc → αβ (幅值不变形式)。
 * 精确复制 chb_abc_to_alphabeta, double → float。
 */
static void clarke(float a, float b, float c, float *alpha, float *beta)
{
    *alpha = (2.0f / 3.0f) * (a - 0.5f * b - 0.5f * c);
    *beta  = (2.0f / 3.0f) * (0.86602540378f * (b - c));
}

/*
 * Park 变换: αβ → dq。
 * 精确复制 chb_alphabeta_to_dq, double → float。
 *   d =  α×cosθ + β×sinθ
 *   q = -α×sinθ + β×cosθ
 */
static void park(float alpha, float beta, float theta, float *d, float *q)
{
    float s = sinf(theta);
    float c = cosf(theta);
    *d = alpha * c + beta * s;
    *q = -alpha * s + beta * c;
}

void PLL_Init(PLL_State *pll)
{
    pll->theta = 0.0f;
    pll->pll_i = 0.0f;
    pll->freq  = 50.0f;
    pll->vd    = 0.0f;
    pll->vq    = 0.0f;
    pll->vmag  = 0.0f;
}

void PLL_Run(PLL_State *pll, float va, float vb, float vc, float ts)
{
    float alpha, beta, vd, vq, mag;

    /* 1. Clarke: abc → αβ */
    clarke(va, vb, vc, &alpha, &beta);

    /* 2. 电网电压幅值门控 — 上电/掉电时停止校正环并 50Hz holdover */
    mag = sqrtf(alpha * alpha + beta * beta);
    pll->vmag = mag;
    /*
     * 与前台锁定判决共用同一个板级门槛：低于该αβ峰值时停止PI校正，
     * 仅保持50Hz角度续航。统一门槛可避免“PLL已校正但前台永不就绪”
     * 或“前台认为就绪而PLL仍处于holdover”的不一致状态。
     */
    if (mag < BOARD_PLL_LOCK_VMAG_MIN_V) {
        pll->vd    = 0.0f;
        pll->vq    = 0.0f;
        pll->pll_i = 0.0f;
        pll->freq  = PLL_OMEGA_NOM / PLL_TWO_PI;
        /* 50Hz holdover: 校正环停止, theta 仍按额定频率转动 —
         * alpha=1 时掉压, 调制参考不会冻结, 失锁回退保持平滑 */
        pll->theta = wrap_2pi(pll->theta + PLL_OMEGA_NOM * ts);
        return;
    }

    /* 3. Park: αβ → dq */
    park(alpha, beta, pll->theta, &vd, &vq);
    pll->vd = vd;
    pll->vq = vq;

    /* 4. PI on vq → 频率修正 → theta 积分 */
    {
        float vq_norm = (vq / mag) * PLL_VQ_PU_SCALE;
        float omega;

        pll->pll_i += PLL_KI_VQ * vq_norm * ts;
        pll->pll_i  = clamp(pll->pll_i, -PLL_INTEGRAL_MAX, PLL_INTEGRAL_MAX);

        omega  = PLL_OMEGA_NOM + PLL_KP_VQ * vq_norm + pll->pll_i;
        omega  = clamp(omega, PLL_FREQ_MIN, PLL_FREQ_MAX);

        pll->theta = wrap_2pi(pll->theta + omega * ts);
        pll->freq  = omega / PLL_TWO_PI;
    }
}
