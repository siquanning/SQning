#ifndef APP_STATE_MACHINE_H
#define APP_STATE_MACHINE_H

// 系统四态状态机：IDLE → SOFT_START → RUNNING → FAULT
// 任何状态均可响应停机命令回到 IDLE

typedef enum {
    STATE_IDLE       = 0,
    STATE_SOFT_START = 1,
    STATE_RUNNING    = 2,
    STATE_FAULT      = 3
} system_state_t;

// 全局变量 — CCS 调试器中可直接观察/修改
extern system_state_t g_system_state;
extern float          g_v2_target;       // V2 目标电压 (V)，默认 50V
extern float          g_soft_start_ms;   // 软启动斜坡时长 (ms)，默认 100ms，范围 10~5000
extern volatile int   g_start_cmd;       // 启动命令：调试器中设为 1 触发 IDLE→SOFT_START
extern volatile int   g_stop_cmd;        // 停机命令：调试器中设为 1 触发任意状态→IDLE
extern volatile int   g_fault_flag;      // 故障标志：调试器中设为 1 触发→FAULT

// 初始化状态机到 IDLE
void state_machine_init(void);

// 每 1ms 调用一次（由 systick 回调驱动）：处理状态变迁、V2_ref 斜坡、LED5
void state_machine_step(void);

#endif
