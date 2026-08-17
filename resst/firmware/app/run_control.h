/* Created by Siquanning */
#ifndef RUN_CONTROL_H
#define RUN_CONTROL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * RunControl — GPIO21 启停按钮消抖 (纯逻辑, 无硬件依赖, host 可测)
 *
 * GPIO21 是自锁/保持型按钮 (高有效):
 *   第一次按下 → 引脚保持 1; 再按一次 → 引脚保持 0。
 *
 * 职责:
 *   - 对称 50ms N-of-N 消抖: 连续高 50ms → 稳定 1, 连续低 50ms → 稳定 0
 *   - 消抖后的稳定电平即用户运行请求:
 *       稳定 1 = RUN 请求, 稳定 0 = STOP 请求
 *   - 不产生沿事件, 不做软件 toggle
 *
 * 不做的事:
 *   - 不操作 PWM, 不读写 GPIO/硬件寄存器, 不裁决启动条件
 *   - 稳定电平如何消费 (FAULT 封锁、重启抑制、RequestRun/Standby)
 *     全部由 run_supervisor 负责
 *
 * 调用约定: 每 10ms 调度拍调用一次 RunControl_Sample(active),
 * active = 1 表示本拍 GPIO21 处于有效电平 (与 BOARD_RUN_BTN_ACTIVE_LEVEL 比较后)。
 */
typedef struct
{
    uint16_t stable;   /* 0 = STOP 请求, 1 = RUN 请求 (消抖后稳定电平) */
    uint16_t cnt_hi;   /* 连续高电平计数, 封顶 BOARD_RUN_BTN_DEBOUNCE_TICKS */
    uint16_t cnt_lo;   /* 连续低电平计数, 封顶 BOARD_RUN_BTN_DEBOUNCE_TICKS */
} RunControl;

/*
 * 上电复位: stable=0 (STOP 请求), 计数清零。
 * 上电是否允许首次启动由 RunSupervisor 的 restart_inhibit 裁决,
 * 与本模块无关。
 */
void RunControl_Init(RunControl *rc);

/*
 * 每 10ms 一拍, 更新对称消抖状态。
 * 连续 50ms 高 → stable=1; 连续 50ms 低 → stable=0;
 * 任一相反电平出现即清零另一侧计数 (保持型开关拨动时的抖动同样被滤除)。
 */
void RunControl_Sample(RunControl *rc, uint16_t active);

/*
 * 读取消抖后的稳定电平: 0 = STOP 请求, 1 = RUN 请求。
 */
uint16_t RunControl_GetStableLevel(const RunControl *rc);

#ifdef __cplusplus
}
#endif

#endif /* RUN_CONTROL_H */
