/* Created by Siquanning */
#ifndef JUSTFLOAT_H
#define JUSTFLOAT_H

#include <stdint.h>
#include "firmware/bsp/board_config.h"

/*
 * JustFloat 协议 (VOFA+) — 周期发送 float 小端数组 + 4 字节帧尾 00 00 80 7F。
 * 帧通道数由 BOARD_DEBUG_WAVEFORM_LITE 决定：
 *   lite=0（完整 VIEW0~10）: 固定 8 通道（36 字节帧），保持固定帧长。
 *   lite=1（轻量波形模式）: 固定 6 通道（28 字节帧），g_jf_lite_mode
 *                           运行时选择 Vac/Iac、Vdc1..Vdc6 或当前相双闭环。
 * 通道来源：统一 DebugSnapshot（firmware/app/debug_snapshot.h）。
 *
 * Lite 波形路径（BOARD_DEBUG_WAVEFORM_LITE=1）：
 *   生产者: 20kHz ISR 的 1kHz 快照分频点 JustFloat_OnSnapshot() 入队，
 *           不依赖 1ms 前台何时运行。
 *   消费者: SCI-C TX FIFO ISR 发完当前帧后立即取下一帧，不忙等 FIFO。
 *   队列满才记 overflow；禁止 busy→drop。
 * 完整 VIEW 路径仍由 1ms JustFloat_Service 组帧提交。
 *
 * 运行期变量（CCS Expressions 可直接在线修改，也可经 SCI-C RX 协议修改）:
 *   g_jf_enable: 0=停止 JustFloat, 1=开启（默认 1）
 *   g_jf_view:   VIEW 编号 0..10（见 board_config.h DEBUG_VIEW_*，默认 0；
 *                lite=1 时忽略）
 *   g_jf_phase:  观测相选择，0=自动跟随 g_ctrl_test_phase, 1=A, 2=B, 3=C
 *                （默认 0；只影响观测，不影响控制相别锁存）
 *   g_jf_lite_mode: 轻量通道组，0=Va/Vb/Vc/Ia/Ib/Ic，1=Vdc1..Vdc6，
 *                   2=Id_ref/Id/Iq/VdcAvg/VdcRefRamp/m（默认 0；仅 lite=1 时有效）
 *
 * VIEW 通道定义（g_jf_view, 仅 lite=0 时有效）:
 *   0 PLL 跟随:     实测Va/Vb/Vc + PLL生成三相跟随波(±120°) + freq + vq
 *   1 PLL 内部:     Va/Vb/Vc + vd/vq/vmag + freq + lock
 *   2 采样:         Ia/Ib/Ic + Va/Vb/Vc + 当前相 Vdc1/Vdc2（全部实际值）
 *   3 Vdc 总览:     Vdc1..Vdc6 + 当前相 VdcAvg + VdcRefRamp
 *   4 Vdc 外环:     Vdc1/Vdc2/VdcAvg/VdcRefRamp/VdcErr/Iamp/VdcIntegral/IampLim
 *   5 dq 内环:      Id_ref/Id/Id_err/Iq_ref/Iq/Iq_err/Vd_ctrl/Vq_ctrl
 *   6 PWM/安全链:   m_final/左桥CMP/右桥CMP/UNI/GPIO30/activePhase/TZ/state
 *   7 启停:         runReq/state/activePhase/pllLock/GPIO30/GPIO42/GPIO44/fault
 *   8 综合:         VdcAvg/Vac/Iac/Id_ref/m_final/freq/state/fault
 *   9 QSG 诊断:     Iac/Ialpha/Ibeta/theta_phase/Id/Iq/freq/activePhase
 *   10 线电压:      Vab/Vbc/Vca + vmag/freq/vq/activePhase/pllLock
 * 其他值：回退到 BOARD_DEBUG_VIEW_DEFAULT。
 */

#if BOARD_DEBUG_WAVEFORM_LITE
#define JUSTFLOAT_CH_COUNT   6U   /* lite: AC / Vdc / 双闭环 六路运行时切换 */
#else
#define JUSTFLOAT_CH_COUNT   8U   /* full: VIEW0~10 */
#endif

/* Lite 环形队列槽位数（空一格，最多同时缓存 LEN-1 帧）。 */
#define JUSTFLOAT_WAVE_QUEUE_LEN  16U

extern volatile uint16_t g_jf_enable;
extern volatile uint16_t g_jf_view;
extern volatile uint16_t g_jf_phase;
extern volatile uint16_t g_jf_lite_mode;

/*
 * JustFloat TX 统计（CCS Expressions 可观察）:
 *   g_jf_sent_count: 完整发送完成的帧数（与 g_waveform_sent_count 同步）。
 *   g_jf_drop_count: 仅完整 VIEW 路径：busy 时提交被丢弃的帧数。
 *                    Lite 路径不再 busy-drop，满队列记 overflow。
 */
extern volatile uint32_t g_jf_sent_count;
extern volatile uint32_t g_jf_drop_count;

/*
 * Lite 波形队列统计（CCS Expressions，10s 目标：
 * produced≈10000, sent≈10000, overflow=0, max_depth<16）:
 */
extern volatile uint32_t g_waveform_produced_count;
extern volatile uint32_t g_waveform_sent_count;
extern volatile uint32_t g_waveform_queue_overflow_count;
extern volatile uint16_t g_waveform_queue_max_depth;

void JustFloat_Send(const float *ch, uint16_t ch_count);
void JustFloat_GetChannels(uint16_t view, float *ch);

/*
 * 20kHz ISR 的 1kHz 快照分频点调用：从 g_dbg_snap 入队一帧。
 * 禁止调用 GetChannels（其 RestoreGlobal=EINT 会在快 ISR 内意外开中断）。
 * 20kHz 路径不写 SCITXBUF，最多武装 TXFFIENA。
 */
#if BOARD_DEBUG_JUSTFLOAT_ENABLE
void JustFloat_OnSnapshot(void);
#else
#define JustFloat_OnSnapshot()
#endif

/* 1ms tick — Lite: 仅协议暂停恢复后 kick TX；完整 VIEW: 组帧提交。
 * 20kHz 控制 ISR 内不写 SCITXBUF。 */
void JustFloat_Service(void);

/*
 * 单一 TX owner 仲裁：协议层（pll_host_protocol GET response 等）在调用
 * DrvSci_SendBytes() 独占发送前必须先调用本函数。内部有界等待当前
 * JustFloat 帧完整发送结束（busy=0，最长一帧 ≈0.63ms@576000），然后
 * 暂停 JustFloat TX（队列仍可入队），保证协议 response 不会被半帧打断。
 */
void JustFloat_TxYieldForProtocol(void);

/* SCI-C TX FIFO 中断服务（PIE 8.6）— 按 GetTxFifoFree() 续搬，帧完成后
 * 立即取队列下一帧。主机测试可直接调用以模拟 TX FIFO 中断推进。 */
#ifdef __TMS320C28XX__
__interrupt void JustFloat_ScicTxIsr(void);
#else
void JustFloat_ScicTxIsr(void);
#endif

#endif
