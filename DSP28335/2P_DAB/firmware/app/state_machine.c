#include "include/common.h"
#include "app/state_machine.h"
#include "bsp/led.h"
#include "control/pid.h"

// 全局状态 — CCS 调试器中可直接观察
system_state_t g_system_state    = STATE_IDLE;
float          g_v2_target       = 50.0f;
float          g_soft_start_ms   = 100.0f;
volatile int   g_start_cmd       = 0;
volatile int   g_stop_cmd        = 0;
volatile int   g_fault_flag      = 0;

// 外部引用 — main.c 中定义的 V2_ref 和 PID 实例
extern float g_v2_ref;
extern pid_t g_pid;

// 斜坡内部变量
static float ramp_step_val;       // 每 1ms 台阶的电压增量
static int   ramp_steps_remain;   // 剩余台阶数

// LED5 闪烁控制
typedef enum { LED_OFF, LED_ON, LED_SLOW, LED_FAST } led_mode_t;
static led_mode_t led5_mode;
static int led5_tick;

#define SLOW_PERIOD_MS  250     // 慢闪周期 500ms → 每 250ms 翻转一次
#define FAST_PERIOD_MS   50     // 快闪周期 100ms → 每 50ms 翻转一次

// 启动斜坡：计算台阶参数，从零开始
static void ramp_start(void)
{
    int steps = (int)(g_soft_start_ms);
    if (steps < 1) steps = 1;
    ramp_steps_remain = steps;
    ramp_step_val     = g_v2_target / (float)steps;

    g_v2_ref = 0.0f;
    pid_reset(&g_pid);
}

void state_machine_init(void)
{
    g_system_state = STATE_IDLE;
    g_start_cmd    = 0;
    g_stop_cmd     = 0;
    g_fault_flag   = 0;
    led5_mode      = LED_OFF;
    led5_tick      = 0;
    led5_off();
}

void state_machine_step(void)
{
    // ---- 1. 停机命令：任意状态 → IDLE（最高优先级）----
    if (g_stop_cmd) {
        g_stop_cmd     = 0;
        g_start_cmd    = 0;
        g_system_state = STATE_IDLE;
        g_v2_ref       = 0.0f;
        pid_reset(&g_pid);
        led5_mode = LED_OFF;
        led5_tick = 0;
        led5_off();
        return;
    }

    // ---- 2. 启动命令：仅 IDLE 可启动 ----
    if (g_start_cmd && g_system_state == STATE_IDLE) {
        g_start_cmd = 0;
        ramp_start();
        g_system_state = STATE_SOFT_START;
        led5_mode = LED_SLOW;
        led5_tick = 0;
    }

    // ---- 3. 故障标志 → FAULT（仅首次进入时重置 LED 节拍）----
    if (g_fault_flag && g_system_state != STATE_IDLE && g_system_state != STATE_FAULT) {
        g_system_state = STATE_FAULT;
        led5_mode = LED_FAST;
        led5_tick = 0;
    }

    // ---- 4. 状态机调度 ----
    switch (g_system_state) {

    case STATE_IDLE:
        led5_mode = LED_OFF;
        break;

    case STATE_SOFT_START:
        led5_mode = LED_SLOW;
        // 斜坡上升
        g_v2_ref += ramp_step_val;
        ramp_steps_remain--;
        if (ramp_steps_remain <= 0 || g_v2_ref >= g_v2_target) {
            g_v2_ref       = g_v2_target;
            g_system_state = STATE_RUNNING;
            led5_mode      = LED_ON;
        }
        break;

    case STATE_RUNNING:
        led5_mode = LED_ON;
        break;

    case STATE_FAULT:
        led5_mode = LED_FAST;
        break;
    }

    // ---- 5. LED5 驱动 ----
    switch (led5_mode) {
    case LED_OFF:
        led5_off();
        break;
    case LED_ON:
        led5_on();
        break;
    case LED_SLOW:
        if (++led5_tick >= SLOW_PERIOD_MS) {
            led5_tick = 0;
            led5_toggle();
        }
        break;
    case LED_FAST:
        if (++led5_tick >= FAST_PERIOD_MS) {
            led5_tick = 0;
            led5_toggle();
        }
        break;
    }
}
