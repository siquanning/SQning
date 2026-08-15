#ifndef CONTROL_PLL_H
#define CONTROL_PLL_H

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

void PLL_Init(PLL_State *pll);
void PLL_Run(PLL_State *pll, float va, float vb, float vc, float ts);

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_PLL_H */
