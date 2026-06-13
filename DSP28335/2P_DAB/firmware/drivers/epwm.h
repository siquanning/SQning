#ifndef DRIVERS_EPWM_H
#define DRIVERS_EPWM_H

#include "include/common.h"
#include "DSP2833x_EPwm_defines.h"

// -----------------------------------------------------------------------------
// PWM 时序参数（SYSCLKOUT=150MHz，TBCLK=SYSCLKOUT，递增-递减模式）
// -----------------------------------------------------------------------------
#define PWM_FREQ_HZ         10000   // 开关频率 10kHz
#define PWM_TBPRD           7500    // TBPRD = 150MHz / (2 × 10kHz) = 7500

// 死区：200ns @150MHz TBCLK → 30 个 TBCLK 时钟周期
#define PWM_DB_TICKS        30

// 同步链模块编号
#define EPWM_MODULE_S1S2    1       // ePWM1: S1/S2（原边左半桥）
#define EPWM_MODULE_S3S4    2       // ePWM2: S3/S4（原边右半桥）
#define EPWM_MODULE_Q1Q2    3       // ePWM3: Q1/Q2（副边左半桥）
#define EPWM_MODULE_Q3Q4    4       // ePWM4: Q3/Q4（副边右半桥）

// -----------------------------------------------------------------------------
// 公开接口
// -----------------------------------------------------------------------------

// 初始化 ePWM1~4：时基、比较器、动作限定、死区、同步链
void epwm_init(void);

// 设置指定模块的占空比，duty ∈ [0, 1]
void epwm_set_duty(Uint16 module, float duty);

// 设置指定模块的相移，phase ∈ [0, 1]，映射到 TBPHS ∈ [0, TBPRD]
void epwm_set_phase(Uint16 module, float phase);

#endif
