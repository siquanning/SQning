#include "include/common.h"
#include "bsp/clock.h"
#include "bsp/gpio.h"
#include "bsp/led.h"
#include "bsp/delay.h"
#include "bsp/systick.h"
#include "drivers/epwm.h"
#include "drivers/adc.h"
#include "control/dps.h"
#include "control/pid.h"
#include "app/state_machine.h"
#include "app/protection.h"

// ---------------------------------------------------------------------------
// 闭环控制参数 — 全部可直接在 CCS 调试器中修改（非 volatile，可传 float*）
// ---------------------------------------------------------------------------
float  g_v2_ref = 50.0f;    // V2 电压给定 (V)，调试器中修改
pid_t  g_pid;                // PID 状态，调试器中观察/修改 Kp/Ki/Kd
float  g_d1, g_d2;           // DPS 输出：桥间/桥内移相比
float  g_sd1, g_sd2, g_st;  // 调制信号（调试用）
float  g_p0;                 // PID 输出功率指令

// PID 默认参数（PRD 默认值）
#define PID_KP_DEFAULT  1.0f
#define PID_KI_DEFAULT  0.1f
#define PID_KD_DEFAULT  1.0f
#define PID_DT          0.001f     // 1ms
#define PID_OUT_MAX     0.999f

// ===========================================================================
// 控制回调 — 由 systick ISR 每 1ms 调用一次（1kHz 控制频率）
// ===========================================================================
static void control_step(void)
{
    // 保护检查：OVP/OCP/TZ1，3 次连续超阈值抗尖峰干扰
    protection_step();

    // 状态机：启停命令、状态变迁、软启动斜坡、LED5 指示
    state_machine_step();

    // IDLE/FAULT 状态下不执行控制算法
    if (g_system_state == STATE_IDLE || g_system_state == STATE_FAULT) {
        return;
    }

    // 1. 读取 ADC 反馈（ADC ISR 10kHz 异步更新，这里只读最新的）
    float v2 = g_adc.v2_filtered;
    float k  = g_adc.k;

    // 2. PID: 电压误差 → 功率指令 p0
    float p0 = pid_step(&g_pid, g_v2_ref, v2);
    g_p0 = p0;

    // 3. DPS: p0, k → D1, D2
    dps_compute(p0, k, &g_d1, &g_d2);

    // 4. 调制信号更新（调试用，不影响实际 PWM）
    dps_modulation_signals(g_d1, g_d2, &g_sd1, &g_sd2, &g_st);

    // 5. 写入 ePWM 相移寄存器（影子寄存器，CTR=0 时生效）
    dps_update_epwm(g_d1, g_d2);
}

// ===========================================================================
// main
// ===========================================================================
void main(void)
{
    // ---- 系统时钟 150MHz ----
    clock_init();

    // ---- 关全局中断，初始化期间禁止响应 ----
    DINT;

    // ---- PIE 控制寄存器和向量表 ----
    InitPieCtrl();
    IER = 0;
    IFR = 0;
    InitPieVectTable();

    // ---- GPIO / LED ----
    gpio_init();
    led_all_on();
    delay_ms(200);
    led_all_off();

    // ---- ePWM: 4 路互补 + 死区，10kHz，同步链 ----
    epwm_init();

    // ---- ADC: ADCINA0 采样 V2，ePWM1 SOCA 触发，10kHz，IIR 滤波 ----
    adc_init();
    EALLOW;
    PieVectTable.ADCINT = &adc_isr;
    EDIS;
    PieCtrlRegs.PIEIER1.bit.INTx6 = 1;  // PIE 组 1，通道 6 = ADCINT

    // ---- PID 初始化 ----
    pid_init(&g_pid, PID_KP_DEFAULT, PID_KI_DEFAULT, PID_KD_DEFAULT,
             PID_DT, 0.0f, PID_OUT_MAX);

    // ---- Systick: CPU Timer 0, 1kHz 控制节拍 ----
    systick_init();
    systick_register_callback(control_step);

    // ---- 状态机：初始化为 IDLE ----
    state_machine_init();

    // ---- 保护逻辑：OVP/OCP 阈值、TZ1 故障锁存 ----
    protection_init();

    // ---- 开全局中断 ----
    EINT;
    ERTM;

    // ---- 主循环：LED1 心跳闪烁（500ms）----
    while (1) {
        led1_toggle();
        delay_ms(500);
    }
}
