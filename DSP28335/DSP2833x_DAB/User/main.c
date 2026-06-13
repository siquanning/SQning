/*
 * main.c — DSP28335 DAB + Modbus RTU 从站
 *
 * - SCI-A（GPIO35/36），9600-8N1，Modbus RTU slave addr=1
 * - 保持寄存器 40001–40008（R/W）
 * - 输入寄存器 30001–30006（R/O）
 * - LED1 心跳，LED2 通信帧指示
 */

#include "DSP2833x_Device.h"
#include "DSP2833x_Examples.h"
#include "types.h"
#include "gpio_config.h"
#include "sci_driver.h"
#include "modbus_slave.h"
#include "app_modbus.h"
#include "leds.h"

// ---- 1ms 系统时基（CPU Timer 0） ----------------------------------------------
// SYSCLK = 150MHz，CPU Timer 周期 = 150MHz / 150000 = 1kHz

#define SYSTICK_PRD  150000UL

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

// ---- PIE 全部清零 ------------------------------------------------------------
// PIEIER1~12 和 PIEIFR1~12 在内存中连续排列（共 24 个 Uint16），
// 清零后只重新使能我们需要的中断线，防止意外触发 DefaultISR。

static void PIE_ClearAll(void)
{
    volatile Uint16 *p = &PieCtrlRegs.PIEIER1.all;
    Uint16 i;
    for (i = 0; i < 24; i++)
        p[i] = 0;
}

// ---- 主函数 ------------------------------------------------------------------

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
    PieVectTable.TINT0     = &cpu_timer0_isr;
    PieVectTable.SCIRXINTA = &Scia_Rx_ISR;
    EDIS;

    // --- 外设初始化 ---
    LED_Init();
    Init_Scia_Gpio();
    Init_Scia();
    Systick_Init();

    // --- Modbus 初始化 ---
    MB_Init();
    MB_InitRegs();

    // --- 启动信号 ---
    LED5_ON;
    DELAY_US(200000);
    LED5_OFF;

    Scia_SendString("DSP28335 DAB Modbus RTU Slave ready\r\n");

    // --- 全局中断 ---
    EINT;
    ERTM;

    // --- 主循环 ---
    while (1)
    {
        if (MB_Poll() > 0)
            LED2_TOGGLE;

        if (ms_counter % 500 == 0)
            LED1_TOGGLE;
    }
}
