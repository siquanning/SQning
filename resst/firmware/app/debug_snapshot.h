/* Created by Siquanning */
#ifndef DEBUG_SNAPSHOT_H
#define DEBUG_SNAPSHOT_H

#include <stdint.h>

/*
 * DEBUG_SNAPSHOT_H — JustFloat 轻量六通道快照（纯观测层）
 *
 * 生产者: App_Epwm1Isr 按 1 kHz 分频更新
 * 消费者: JustFloat_GetChannels() 在 DINT 保护下拷贝
 *
 *   vac[]      重构相电压（与 g_pll_input_vabc / PLL 输入同源）
 *   vline[]    实测线电压 Vab/Vbc/Vca（与 g_pll_input_vline 同源）
 *   pll_vmag/theta  供 mode 2 在 1ms 前台生成跟随波（ISR 内不算 cosf）
 */

typedef struct
{
    float vac[3];       /* Va/Vb/Vc [V]，重构相电压 */
    float vline[3];     /* Vab/Vbc/Vca [V]，实测线电压 */
    float iac[3];       /* Ia/Ib/Ic [A] */
    float vdc[6];       /* Vdc1..Vdc6 [V] */
    float pll_vmag;     /* g_pll.vmag [V] */
    float pll_theta;    /* g_pll.theta [rad] */
} DebugSnapshot;

extern DebugSnapshot g_dbg_snap;

/* GPIO21 消抖稳定电平的运行期镜像（app.c 定义/更新） */
extern volatile uint16_t g_run_request;

#endif /* DEBUG_SNAPSHOT_H */
