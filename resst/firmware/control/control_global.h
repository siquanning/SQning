/* Created by Siquanning */
#ifndef CONTROL_GLOBAL_H
#define CONTROL_GLOBAL_H

#include <stdint.h>
#include "firmware/control/control_pll.h"

#ifdef __cplusplus
extern "C" {
#endif

extern PLL_State g_pll;
/* PLL本拍输入：重构后的相电压 Va/Vb/Vc（仿真路径则直接为相电压）。 */
extern volatile float g_pll_input_vabc[3];
/* 本拍实测线电压 Vab/Vbc/Vca（仿真路径由相电压相减得到）。 */
extern volatile float g_pll_input_vline[3];

/* ---- PLL 软切换全局状态 ----
 * g_pll_switch_req: 前台 10ms 锁定判决写入 (0=回开环, 1=切 PLL)，ISR 只读
 * g_switch_alpha:   ISR 维护的淡化系数 (0=纯开环, 1=纯 PLL)，前台诊断读取
 * g_switch_phase_err_deg: ISR 写入，PLL 目标相位 − 输出相位 [°]，诊断用
 */
extern volatile uint16_t g_pll_switch_req;
extern volatile float    g_switch_alpha;
extern volatile float    g_switch_phase_err_deg;

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_GLOBAL_H */
