/* Created by Siquanning */
#ifndef CONTROL_PLL_H
#define CONTROL_PLL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 三相电网 SRF-PLL — Clarke/Park + PI on vq → theta.
 * 每 20kHz ISR 调用一次 PLL_Run()，输入三相瞬时电网电压 [V]。
 *
 * 参数来自 CHB 级联 H 桥整流器原始设计:
 *   ki=2000, kp=120, omega_nom=2π×50, freq clamp [45,55]Hz
 */

typedef struct {
    float theta;       /* 锁相角 [0, 2π) */
    float pll_i;       /* PI 积分项（频率修正量）[rad/s] */
    float freq;        /* 当前估计频率 [Hz] */
    float vd;          /* d 轴电压 [V] */
    float vq;          /* q 轴电压 [V] */
    float vmag;        /* αβ 矢量幅值 [V]，诊断用 */
} PLL_State;

typedef struct {
    float kp;
    float ki;
    float freq_min_hz;
    float freq_max_hz;
    float freq_nom_hz;
    float vq_lock_ratio;
    float vq_unlock_ratio;
} PLL_Params;

void PLL_GetDefaultParams(PLL_Params *params);
uint16_t PLL_ValidateParams(const PLL_Params *params);
void PLL_CommitParams(const PLL_Params *params);
void PLL_ReadActiveParams(PLL_Params *params);

void PLL_Init(PLL_State *pll);
void PLL_Run(PLL_State *pll, float va, float vb, float vc, float ts);

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_PLL_H */
