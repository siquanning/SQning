/* Created by Siquanning */
#include "firmware/control/control_pll.h"
#include "firmware/bsp/board_config.h"
#include <math.h>

/*
 * PLL 参数 — 来自 CHB 级联 H 桥整流器原始设计。
 * 原设计在311V峰值下使用vq×0.001。低压测试改为(vq/vmag)×0.311，
 * 保持原额定点环路增益，同时避免锁相动态随输入幅值降低而显著变慢。
 */
#define PLL_VQ_PU_SCALE   0.311f       /* (vq/vmag) → 原311V额定点尺度 */
#define PLL_TWO_PI         6.283185307f

static PLL_Params s_param_bank[2] = {
    {BOARD_PLL_KP_DEFAULT, BOARD_PLL_KI_DEFAULT,
     BOARD_PLL_FREQ_MIN_HZ_DEFAULT, BOARD_PLL_FREQ_MAX_HZ_DEFAULT,
     BOARD_PLL_FREQ_NOM_HZ_DEFAULT, BOARD_PLL_VQ_LOCK_RATIO_DEFAULT,
     BOARD_PLL_VQ_UNLOCK_RATIO_DEFAULT},
    {BOARD_PLL_KP_DEFAULT, BOARD_PLL_KI_DEFAULT,
     BOARD_PLL_FREQ_MIN_HZ_DEFAULT, BOARD_PLL_FREQ_MAX_HZ_DEFAULT,
     BOARD_PLL_FREQ_NOM_HZ_DEFAULT, BOARD_PLL_VQ_LOCK_RATIO_DEFAULT,
     BOARD_PLL_VQ_UNLOCK_RATIO_DEFAULT}
};
static volatile uint16_t s_active_param_bank;

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

static uint16_t finite_local(float x)
{
    return ((x == x) && (x <= 3.402823466e+38F) &&
            (x >= -3.402823466e+38F)) ? 1U : 0U;
}

void PLL_GetDefaultParams(PLL_Params *p)
{
    if (p == ((PLL_Params *)0)) return;
    p->kp = BOARD_PLL_KP_DEFAULT;
    p->ki = BOARD_PLL_KI_DEFAULT;
    p->freq_min_hz = BOARD_PLL_FREQ_MIN_HZ_DEFAULT;
    p->freq_max_hz = BOARD_PLL_FREQ_MAX_HZ_DEFAULT;
    p->freq_nom_hz = BOARD_PLL_FREQ_NOM_HZ_DEFAULT;
    p->vq_lock_ratio = BOARD_PLL_VQ_LOCK_RATIO_DEFAULT;
    p->vq_unlock_ratio = BOARD_PLL_VQ_UNLOCK_RATIO_DEFAULT;
}

uint16_t PLL_ValidateParams(const PLL_Params *p)
{
    if (p == ((const PLL_Params *)0)) return 0U;
    if (!finite_local(p->kp) || !finite_local(p->ki) ||
        !finite_local(p->freq_min_hz) || !finite_local(p->freq_max_hz) ||
        !finite_local(p->freq_nom_hz) || !finite_local(p->vq_lock_ratio) ||
        !finite_local(p->vq_unlock_ratio)) return 0U;
    return (p->kp >= BOARD_PLL_KP_MIN && p->kp <= BOARD_PLL_KP_MAX &&
            p->ki >= BOARD_PLL_KI_MIN && p->ki <= BOARD_PLL_KI_MAX &&
            p->freq_min_hz >= BOARD_PLL_FREQ_MIN_ALLOWED_HZ &&
            p->freq_max_hz <= BOARD_PLL_FREQ_MAX_ALLOWED_HZ &&
            p->freq_min_hz < p->freq_nom_hz &&
            p->freq_nom_hz < p->freq_max_hz &&
            p->vq_lock_ratio >= BOARD_PLL_VQ_RATIO_MIN &&
            p->vq_unlock_ratio <= BOARD_PLL_VQ_RATIO_MAX &&
            p->vq_lock_ratio < p->vq_unlock_ratio) ? 1U : 0U;
}

void PLL_CommitParams(const PLL_Params *p)
{
    uint16_t next;
    if (PLL_ValidateParams(p) == 0U) return;
    next = (s_active_param_bank == 0U) ? 1U : 0U;
    s_param_bank[next] = *p;
    s_active_param_bank = next;
}

void PLL_ReadActiveParams(PLL_Params *p)
{
    uint16_t index;
    if (p == ((PLL_Params *)0)) return;
    index = s_active_param_bank;
    *p = s_param_bank[index];
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
    PLL_Params defaults;
    PLL_GetDefaultParams(&defaults);
    s_param_bank[0] = defaults;
    s_param_bank[1] = defaults;
    s_active_param_bank = 0U;
    pll->theta = 0.0f;
    pll->pll_i = 0.0f;
    pll->freq  = defaults.freq_nom_hz;
    pll->vd    = 0.0f;
    pll->vq    = 0.0f;
    pll->vmag  = 0.0f;
}

void PLL_Run(PLL_State *pll, float va, float vb, float vc, float ts)
{
    float alpha, beta, vd, vq, mag;
    PLL_Params params;
    PLL_ReadActiveParams(&params);

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
        pll->freq  = params.freq_nom_hz;
        /* 50Hz holdover: 校正环停止, theta 仍按额定频率转动 —
         * alpha=1 时掉压, 调制参考不会冻结, 失锁回退保持平滑 */
        pll->theta = wrap_2pi(pll->theta +
                              params.freq_nom_hz * PLL_TWO_PI * ts);
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
        float omega_nom = params.freq_nom_hz * PLL_TWO_PI;
        float integral_max_hz = params.freq_max_hz - params.freq_nom_hz;
        if ((params.freq_nom_hz - params.freq_min_hz) > integral_max_hz)
            integral_max_hz = params.freq_nom_hz - params.freq_min_hz;

        pll->pll_i += params.ki * vq_norm * ts;
        pll->pll_i  = clamp(pll->pll_i,
                            -integral_max_hz * PLL_TWO_PI,
                             integral_max_hz * PLL_TWO_PI);

        omega  = omega_nom + params.kp * vq_norm + pll->pll_i;
        omega  = clamp(omega, params.freq_min_hz * PLL_TWO_PI,
                              params.freq_max_hz * PLL_TWO_PI);

        pll->theta = wrap_2pi(pll->theta + omega * ts);
        pll->freq  = omega / PLL_TWO_PI;
    }
}
