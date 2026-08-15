#ifndef RUN_SUPERVISOR_H
#define RUN_SUPERVISOR_H

#include <stdint.h>
#include "firmware/app/state_machine.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * RunSupervisor — 启停裁决层 (GPIO21 稳定电平 × 状态机 → PWM/GPIO20)
 *
 * 语义:
 *   - GPIO21 消抖稳定电平 = 用户运行请求 (1=RUN, 0=STOP), 来自 RunControl
 *   - state_machine.state = 系统实际状态 (STANDBY/RUN/FAULT/...)
 *   - PWM 输出只在明确的 START / STOP / FAULT 动作中控制,
 *     不维护任何影子状态变量
 *
 * restart_inhibit (重启锁存):
 *   - 上电初始为 1: 必须先观察到 GPIO21 稳定为 0, 之后 0→1 才允许首次启动
 *   - 发生过 FAULT 即锁存, FAULT 期间 GPIO21 保持 1 也绝不启动;
 *     直到 FAULT 消除后 GPIO21 消抖稳定回到 0 才解除
 *   - 预充超时启动失败同样锁存: 必须 0→1 重新请求才可重试
 *
 * 当BOARD_LOW_VOLTAGE_DIRECT_TEST=1时跳过PRECHARGE；RUN请求到达时锁存
 * 合法运行模式（单相时同时锁存测试相），然后将
 * GPIO22/23同拍闭合，PWM保持OST，再等待PLL就绪与TZ正常，最后按锁存模式
 * 释放目标相两路PWM或三相全部六路PWM。RUN期间请求变量热改不生效。
 * STOP/FAULT时先封锁PWM，再将GPIO22/23同时命令断开。
 * 常规模式启动序列 (seq_state, 主状态机保持 STANDBY):
 *   START (STANDBY) → PRECHARGE → BYPASS_WAIT
 *   → (旁路延时完成 + PLL 就绪 + TZ 正常)
 *   → RequestRun + PWM_ReleaseOutput + LED 亮
 *   预充期间 PWM 始终保持封锁; STOP/FAULT/超时任一时刻立即中止序列。
 *
 * 裁决顺序 (每 10ms 拍, RunSupervisor_Service):
 *   1. FAULT        → 封锁 PWM + LED 灭 + 开关全断 + 锁存 restart_inhibit
 *   2. 抑制期       → 保持封锁, 稳定 0 才解除抑制
 *   3. STOP 请求    → PWM_BlockOutput 第一动作, 开关全断, 序列清零,
 *                     RUN→STANDBY, LED 灭 (同拍)
 *   4. RUN 请求     → IDLE: 进入 PRECHARGE; BYPASS_WAIT 延时完成且 PLL/TZ 就绪:
 *                     RequestRun + Release + LED 亮
 *
 * 序列推进 (每 1ms 拍, RunSupervisor_Service1ms):
 *   PRECHARGE:   vdc_min ≥ g_precharge_done_v → BYPASS_WAIT;
 *                超时 → 中止 + 失败锁存 + restart_inhibit
 *   BYPASS_WAIT: 延时满足后留在本状态等待 PLL/TZ 就绪
 *
 * 开关命令 (GPIO22/GPIO23, CCS Expressions 可观察):
 *   IDLE/STOP:         grid=0, bypass=0, PWM=Block
 *   PRECHARGE:         grid=1, bypass=0, PWM=Block
 *   BYPASS_WAIT/RUN: grid=1, bypass=1 (RUN 时 PWM=Release)
 */

/* ---- 不控整流预充启动子状态 ---- */
typedef enum
{
    START_SEQ_IDLE        = 0,   /* 无启动序列 (上电/STOP/完成/失败后) */
    START_SEQ_PRECHARGE   = 1,   /* S1~S3 ON, S4~S6 OFF, 等 6 路 Vdc ≥ 门槛 */
    START_SEQ_BYPASS_WAIT = 2    /* S1~S6 全 ON, 等旁路延时及 PLL/TZ 就绪 */
} StartSeqState;

typedef struct
{
    uint16_t     restart_inhibit;  /* 1 = 发生过 FAULT/上电/预充超时, 需稳定 0 才解除 */
    StartSeqState seq_state;       /* 预充启动子状态 (仅后台 10ms/1ms 服务访问) */
    uint32_t     seq_entry_tick;   /* 子状态进入时刻 (Timebase_Now 100µs tick) */
} RunSupervisor;

/*
 * 启动现场运行参数，均可在CCS Expressions在线修改。
 * DSP复位后分别恢复为board_config.h中的对应*_DEFAULT宏。
 * 状态机只读取这些运行变量，实验电压或时间变化不需要修改状态机代码。
 */
extern volatile float    g_precharge_done_v;
extern volatile uint32_t g_precharge_timeout_ms;
extern volatile uint32_t g_bypass_delay_ms;
extern volatile float    g_pll_ready_alpha_min;

/* ---- GPIO22/GPIO23 开关命令镜像 (CCS 可观察) ---- */
extern volatile uint16_t g_grid_switch_cmd;     /* S1/S2/S3 三相输入开关 */
extern volatile uint16_t g_bypass_switch_cmd;   /* S4/S5/S6 预充电阻旁路开关 */

/* ---- 启动诊断：只读观察，现场不应通过CCS改写 ---- */
extern volatile uint16_t g_start_seq_fail;       /* 1=本次预充曾超时，下一次START时清零 */
extern volatile float    g_precharge_vdc_min;    /* 六路换算后Vdc的最低值，单位V，1ms刷新 */
extern volatile uint16_t g_start_seq_state;      /* StartSeqState数值镜像：0/1/2 */
extern volatile uint32_t g_start_seq_timer_ms;   /* 当前启动子状态已持续时间，单位ms */

void RunSupervisor_Init(RunSupervisor *rs);

/*
 * 每 10ms 一拍。run_request 为 RunControl_GetStableLevel() 的结果。
 * 内部直接调用 PWM_BlockOutput/PWM_ReleaseOutput/DrvGpio_WriteRunState。
 */
void RunSupervisor_Service(RunSupervisor *rs, StateMachine *sm,
                           uint16_t run_request, uint32_t now);

/*
 * 每 1ms 一拍 — 启动序列推进:
 *   计算 g_precharge_vdc_min; PRECHARGE 门槛/超时判决; BYPASS_WAIT 延时判决。
 * FAULT 期间不推进序列 (开关命令保持安全状态)。
 */
void RunSupervisor_Service1ms(RunSupervisor *rs, StateMachine *sm, uint32_t now);

#ifdef __cplusplus
}
#endif

#endif /* RUN_SUPERVISOR_H */
