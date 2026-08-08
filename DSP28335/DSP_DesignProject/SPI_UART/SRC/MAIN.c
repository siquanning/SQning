/**
 * @file    MAIN.c
 * @brief   工程入口文件 — DSP28335 通用模板
 *
 * 系统初始化流程:
 *   InitSysCtrl()        → 配置 PLL(150MHz), 看门狗, 外设时钟
 *   InitXintf16Gpio()    → 配置 XINTF GPIO (如需外部存储器/ADC)
 *   InitXintf()          → 配置 XINTF 时序
 *   DINT                 → 关全局中断
 *   InitPieCtrl()        → 初始化 PIE 控制寄存器
 *   InitPieVectTable()   → 初始化 PIE 向量表 (默认 ISR)
 *   [注册用户 ISR]       → PieVectTable.xxx = &YourISR;
 *   InitCpuTimers()      → 初始化 CPU 定时器
 *   ConfigCpuTimer()     → 配置定时器周期
 *   [外设初始化]         → ePWM, ADC, SCI, SPI, ... 等
 *   EINT; ERTM           → 开全局中断和实时调试中断
 *
 * 中断服务例程 (ISR) 约定:
 *   - 必须在 ISR 末尾清除 PIE 中断应答: PieCtrlRegs.PIEACK.all = PIEACK_GROUPx;
 *   - CPU Timer ISR 中必须清除定时器标志: CpuTimer0Regs.TCR.bit.TIF=1;
 *   - ISR 应保持简短, 复杂计算放在主循环
 */

#include "DSP2833x_Device.h"
#include "DSP2833x_Examples.h"
#include "APP_CONFIG.h"

/* ---- 全局变量定义 ---- */
/* 在此声明跨 ISR/主循环的共享变量, 使用 volatile 修饰 */

/* ---- 函数声明 ---- */
interrupt void ISRTimer0(void);   /* CPU Timer0 中断服务例程 */
/* TODO: 声明其他 ISR */

/**
 * @brief 主函数 — 初始化后进入空循环
 *        实时控制在 ISR 中完成
 */
void main(void)
{
    /* 1. 系统初始化 */
    InitSysCtrl();

    /* TODO: 如果使用 XINTF, 取消以下注释 */
    // InitXintf16Gpio();
    // InitXintf();

    DINT;

    /* 2. PIE 初始化 */
    InitPieCtrl();
    IER = 0x0000;
    IFR = 0x0000;
    InitPieVectTable();

    /* 3. 注册中断服务例程 */
    EALLOW;
    PieVectTable.TINT0 = &ISRTimer0;
    EDIS;

    /* 使能 CPU 中断 (TINT0 → PIE Group1 → CPU INT1) */
    IER |= M_INT1;

    /* 4. 外设初始化 */
    AppConfig_Init();

    /* 5. 使能中断 */
    EINT;
    ERTM;

    /* 6. 主循环 — 空转 (实时逻辑在 ISR 中完成) */
    for (;;)
    {
        /* TODO Step 4.1: ISR 中轮询 SCI/SPI 收发 */
    }
}

/**
 * @brief Timer0 中断服务例程模板
 *
 * 接口约定:
 *   - 调用频率: 由 ConfigCpuTimer() 的周期参数决定
 *   - 必须清除 TIF 标志和 PIEACK
 *   - 在此执行实时控制算法 (PWM 占空比更新, ADC 读取, 控制环路等)
 */
interrupt void ISRTimer0(void)
{
    /* 清除定时器中断标志 */
    CpuTimer0Regs.TCR.bit.TIF = 1;
    CpuTimer0Regs.TCR.bit.TRB = 1;

    /* TODO: 在此编写实时控制逻辑 */

    /* 清除 PIE 中断应答 */
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
}

/* ---- 用户函数实现 ---- */
/* TODO: 在此添加驱动层和应用层函数 */