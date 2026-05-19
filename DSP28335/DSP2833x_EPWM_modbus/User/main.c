/*
 * main.c — DSP28335 ePWM + Modbus RTU Slave
 *
 * 功能概述:
 *   - ePWM6A (GPIO10): 1kHz / 50% 占空比, 可通过 Modbus 在线调节
 *   - Modbus RTU 从站 (SCI-A, 9600-8-N-1, 地址=1)
 *   - LED1~5 (GPIO64-68): 运行状态指示
 *
 * Modbus 寄存器映射:
 *   40001 (Holding[0]): PWM 频率 (Hz),      R/W, 范围 10–50000
 *   40002 (Holding[1]): PWM 占空比 (0.1%),   R/W, 范围 0–1000
 *   30001 (Input[0]):   实际 PWM 频率 (Hz),  R/O
 *   30002 (Input[1]):   实际 PWM 占空比,     R/O
 */

// ---- 模块头文件 --------------------------------
#include "DSP2833x_Device.h"
#include "DSP2833x_Examples.h"
#include "types.h"
#include "gpio_config.h"
#include "sci_driver.h"
#include "epwm_config.h"
#include "modbus_slave.h"
#include "app_modbus.h"
#include "leds.h"

// ---- 1ms 系统时基 ------------------------------
// SYSCLK = 150MHz, HSPCLK = SYSCLK (HISPCP=0)
// CPU Timer 周期 = 150MHz / 150000 = 1000Hz = 1ms

#define SYSTICK_PRD  150000UL   // 1ms @ 150MHz

volatile Uint32 ms_counter = 0;

static void Systick_Init(void)
{
    EALLOW;
    SysCtrlRegs.PCLKCR3.bit.CPUTIMER0ENCLK = 1;
    EDIS;

    CpuTimer0Regs.TPR.all  = 0;
    CpuTimer0Regs.TPRH.all = 0;
    CpuTimer0Regs.TIM.all  = 0;
    CpuTimer0Regs.PRD.all  = SYSTICK_PRD;

    CpuTimer0Regs.TCR.bit.TSS = 0;   // 启动
    CpuTimer0Regs.TCR.bit.TRB = 1;   // 重载
    CpuTimer0Regs.TCR.bit.TIE = 1;   // 使能中断
    CpuTimer0Regs.TCR.bit.TIF = 1;   // 清标志

    IER |= M_INT1;
    PieCtrlRegs.PIEIER1.bit.INTx7 = 1;
}

__interrupt void cpu_timer0_isr(void)
{
    ms_counter++;
    CpuTimer0Regs.TCR.bit.TIF = 1;
    PieCtrlRegs.PIEACK.bit.ACK1 = 1;
}

// ---- PIE 全部清零 ---------------------------------
// PIEIER1~12 和 PIEIFR1~12 在内存中连续排列 (共 24 个 Uint16)
// 清零后只重新使能我们用到的中断线，其余保持禁用，
// 防止意外触发 DefaultISR 中的 ESTOP0

static void PIE_ClearAll(void)
{
    volatile Uint16 *p = &PieCtrlRegs.PIEIER1.all;
    Uint16 i;
    for (i = 0; i < 24; i++)
        p[i] = 0;
}

// ---- 主函数 ------------------------------------

void main(void)
{
    // --- 系统初始化 ---
    InitSysCtrl();
    DINT;
    InitPieCtrl();
    IER = 0; IFR = 0;
    InitPieVectTable();
    PIE_ClearAll();

    // --- 中断向量映射 ---
    EALLOW;
    PieVectTable.TINT0     = &cpu_timer0_isr;   // INT1.7
    PieVectTable.SCIRXINTA = &Scia_Rx_ISR;       // INT9.1
    EDIS;

    // --- 外设初始化 ---
    LED_Init();
    Init_Scia_Gpio();
    Init_EPWM6_1kHz_50Percent();
    Init_Scia();
    Systick_Init();

    // --- Modbus 初始化 ---
    MB_Init();
    MB_InitRegs();

    // --- 启动信号 ---
    LED1_On();
    DELAY_US(200000);
    LED1_Off();

    // --- 所有外设初始化完毕，开启全局中断 ---
    EINT;
    ERTM;

    // --- 主循环 ---
    while (1)
    {
        MB_Poll();

        if (ms_counter % 500 == 0)
            LED1_Toggle();
    }
}
