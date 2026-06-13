#include "control/dps.h"
#include "drivers/epwm.h"
#include <math.h>

// ============================================================================
// DPS 核心算法 — 来自 PLECS 仿真 dab_test.plecs (C-Script OutputFcn)
// 使用 float (FPU32 硬件加速)，避免 double 的软件模拟开销
// ============================================================================

void dps_compute(float p0, float k, float *d1, float *d2)
{
    const float tol = 1e-6f;

    // 输入限幅
    p0 = fmaxf(0.0f, fminf(1.0f, p0));
    k  = fmaxf(tol, k);

    // 公共判断条件项
    float sqrt_inner = fmaxf(0.0f, 4.0f - 6.0f * p0);
    float sqrt_term  = sqrtf(sqrt_inner);

    // k 的四个边界值
    float bound1_L = (1.0f - sqrt_term) / 3.0f;
    float bound1_H = (1.0f + sqrt_term) / 3.0f;
    float bound2_L = 3.0f / fmaxf(1.0f + sqrt_term, tol);
    float bound2_H = 3.0f / fmaxf(1.0f - sqrt_term, tol);

    float D1 = 0.0f, D2 = 0.0f;

    if (p0 >= (2.0f / 3.0f) && p0 <= 1.0f) {
        // 【区域 1】高功率区间
        if (k >= 1.0f) {
            float denom = fmaxf(tol, 2.0f * (k*k - 2.0f*k + 3.0f));
            float A = sqrtf(fmaxf(0.0f, 1.0f - p0) / denom);
            D1 = (k - 1.0f) * A;
            D2 = 0.5f - A;
        } else {
            float denom = fmaxf(tol, 2.0f * (3.0f*k*k - 2.0f*k + 1.0f));
            float A = sqrtf(fmaxf(0.0f, 1.0f - p0) / denom);
            D1 = (1.0f - k) * A;
            D2 = 0.5f - k * A;
        }
    } else {
        // 【区域 2 & 3】低功率区间 (p0 < 2/3)
        if (k >= bound1_L && k < bound1_H) {
            // 【区域 2】k 接近 1 的中间区域
            float t1 = p0 * fmaxf(0.0f, 1.0f - k)
                       / fmaxf(tol, 2.0f * (1.0f + 3.0f * k));
            float t2_den = fmaxf(tol, sqrtf(fmaxf(0.0f,
                2.0f * p0 * (1.0f - k) * (1.0f + 3.0f * k))));
            float t2 = 2.0f * k * p0 / t2_den;
            D1 = 1.0f - sqrtf(t1) - t2;
            D2 = sqrtf(t1);
        } else if (k >= bound2_L && k < bound2_H) {
            // 【区域 3】k >= 1 时的中间区域
            float t1 = p0 * fmaxf(0.0f, k - 1.0f)
                       / fmaxf(tol, 2.0f * (k + 3.0f));
            float t2_den = fmaxf(tol, sqrtf(fmaxf(0.0f,
                2.0f * p0 * (k*k + 2.0f*k - 3.0f))));
            float t2 = 2.0f * p0 / t2_den;
            D1 = 1.0f - sqrtf(t1) - t2;
            D2 = sqrtf(t1);
        } else {
            // 【兜底】跳出中间区域，回退到高功率计算逻辑
            if (k >= 1.0f) {
                float denom = fmaxf(tol, 2.0f * (k*k - 2.0f*k + 3.0f));
                float A = sqrtf(fmaxf(0.0f, 1.0f - p0) / denom);
                D1 = (k - 1.0f) * A;
                D2 = 0.5f - A;
            } else {
                float denom = fmaxf(tol, 2.0f * (3.0f*k*k - 2.0f*k + 1.0f));
                float A = sqrtf(fmaxf(0.0f, 1.0f - p0) / denom);
                D1 = (1.0f - k) * A;
                D2 = 0.5f - k * A;
            }
        }
    }

    // 输出限幅
    *d1 = fmaxf(0.0f, fminf(1.0f, D1));
    *d2 = fmaxf(0.0f, fminf(1.0f, D2));
}

// ============================================================================
// 调制信号计算 (PLECS 载波比较等效)
// ============================================================================

void dps_modulation_signals(float d1, float d2, float *sd1, float *sd2, float *st)
{
    *sd1 = 0.5f * (1.0f + d1);
    *sd2 = 0.5f * d2;
    *st  = *sd1 + *sd2;
}

// ============================================================================
// D1/D2 → ePWM TBPHS 相移值映射
//
// DPS 约定: D2 是桥内两腿之间的相移量, D1 是原/副边桥间相移。
//   D2=0   → 两腿同相, v_bridge=0 (零功率)
//   D2=0.5 → 两腿180°反相, v_bridge=满方波 (全功率)
//
// 桥臂分配:
//   ePWM1: S1/S2 — 原边左半桥 (参考 phase=0)
//   ePWM2: S3/S4 — 原边右半桥 (phase=D2)
//   ePWM3: Q1/Q2 — 副边左半桥 (phase=D1)
//   ePWM4: Q3/Q4 — 副边右半桥 (phase=D1+D2)
//
// 递增-递减模式, phase ∈ [0,1] → TBPHS = phase × TBPRD.
// phase=0.5 对应 180° (TBPHS=TBPRD).
// ============================================================================

static float wrap_phase(float ph)
{
    while (ph >= 1.0f) ph -= 1.0f;
    while (ph < 0.0f) ph += 1.0f;
    return ph;
}

void dps_update_epwm(float d1, float d2)
{
    // ePWM1: S1/S2 参考
    epwm_set_phase(EPWM_MODULE_S1S2, 0.0f);

    // ePWM2: S3/S4, 相对 S1 相移 D2
    epwm_set_phase(EPWM_MODULE_S3S4, wrap_phase(d2));

    // ePWM3: Q1/Q2, 相对 S1 相移 D1 (桥间移相)
    epwm_set_phase(EPWM_MODULE_Q1Q2, wrap_phase(d1));

    // ePWM4: Q3/Q4, 相对 S1 相移 D1+D2
    epwm_set_phase(EPWM_MODULE_Q3Q4, wrap_phase(d1 + d2));
}
