/* Created by Siquanning */
#ifndef JUSTFLOAT_H
#define JUSTFLOAT_H

#include <stdint.h>
#include "firmware/bsp/board_config.h"

/*
 * JustFloat 协议 (VOFA+) — 周期发送 float 小端数组 + 4 字节帧尾 00 00 80 7F。
 * 固定 6 通道（28 字节帧），g_jf_lite_mode 运行时选择通道组：
 *   0 = 实测线电压 Vab/Vbc/Vca + Ia/Ib/Ic
 *   1 = Vdc1..Vdc6
 *   2 = 重构相电压 Va/Vb/Vc + PLL 三相跟随波
 *
 * 通道来源：统一 DebugSnapshot（firmware/app/debug_snapshot.h），
 * 由 20kHz 控制 ISR 按 1kHz 分频更新，1ms 前台 JustFloat 只读快照。
 *
 * 运行期变量（CCS Expressions 可直接在线修改，也可经 SCI-C RX 协议修改）:
 *   g_jf_enable:    0=停止 JustFloat, 1=开启（默认 1）
 *   g_jf_phase:     观测相 0=自动 / 1..3=A/B/C（保留协议，当前六通道组不使用）
 *   g_jf_lite_mode: 0=线电压+电流，1=Vdc1..Vdc6，2=相电压+PLL跟随
 */

#define JUSTFLOAT_CH_COUNT   6U

extern volatile uint16_t g_jf_enable;
extern volatile uint16_t g_jf_phase;
extern volatile uint16_t g_jf_lite_mode;

/*
 * JustFloat TX 统计（CCS Expressions 可观察）:
 *   g_jf_sent_count: 完整发送完成的帧数 — 在最终 TX FIFO empty
 *                    ISR（整帧最后一字节已物理发出、busy 清零）中递增。
 *   g_jf_drop_count: 因上一帧尚未发送完而被丢弃的帧数（busy 时提交）。
 */
extern volatile uint32_t g_jf_sent_count;
extern volatile uint32_t g_jf_drop_count;

void JustFloat_Send(const float *ch, uint16_t ch_count);
void JustFloat_GetChannels(float *ch);

/* 1ms tick — 发送周期由BOARD_JUSTFLOAT_PERIOD_MS配置，当前1kHz。
 * 20kHz 控制 ISR 内不做任何 SCI 发送。 */
void JustFloat_Service(void);

/*
 * 单一 TX owner 仲裁：协议层（pll_host_protocol GET response 等）在调用
 * DrvSci_SendBytes() 独占发送前必须先调用本函数。内部有界等待当前
 * JustFloat 帧完整发送结束（busy=0，最长一帧 ≈0.63ms@576000），然后
 * 暂停一次 JustFloat（下一帧让路），保证协议 response 不会被 JustFloat
 * 半帧打断，response 尾字节与下一 JustFloat 帧之间至少间隔 1ms。
 */
void JustFloat_TxYieldForProtocol(void);

/* SCI-C TX FIFO 中断服务（PIE 8.6）— 继续搬运剩余帧字节到 TX FIFO。
 * 主机测试可直接调用以模拟 FIFO 空中断推进。 */
#ifdef __TMS320C28XX__
__interrupt void JustFloat_ScicTxIsr(void);
#else
void JustFloat_ScicTxIsr(void);
#endif

#endif
