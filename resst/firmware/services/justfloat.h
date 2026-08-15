#ifndef JUSTFLOAT_H
#define JUSTFLOAT_H

#include <stdint.h>

/*
 * JustFloat 协议 (VOFA+) — 周期发送 float 小端数组 + 4 字节帧尾 00 00 80 7F。
 *
 * g_debug_view=1: CH0=Va, CH1=Vb, CH2=Vc, CH3=theta, CH4=freq,
 *                 CH5=vq, CH6=vmag, CH7=alpha。
 * g_debug_view=2: CH0=当前相Vac, CH1=当前相Iac, CH2=Iref, CH3=Vdc_avg,
 *                 CH4=Vdc_ref_ramp, CH5=Iamp, CH6=m, CH7=theta_phase。
 * 其他值：回退到BOARD_DEBUG_VIEW_DEFAULT，不创建第三种隐式视图。
 */

#define JUSTFLOAT_CH_COUNT   8U

extern volatile uint16_t g_debug_view;
extern volatile uint16_t g_debug_phase;

void JustFloat_Send(const float *ch, uint16_t ch_count);
void JustFloat_GetChannels(uint16_t view, float *ch);

/* 1ms tick — 每4次调用发一帧，输出周期4ms（250Hz波形观察）。 */
void JustFloat_Service(void);

#endif
