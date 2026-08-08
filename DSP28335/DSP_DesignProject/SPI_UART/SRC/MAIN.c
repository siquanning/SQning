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

    /* 3. 注册中断服务例程 (AppConfig_InitXxx 中通过 EALLOW/EDIS 完成) */

    /* 4. 外设初始化 */
    InitCpuTimers();
    /* TODO: 配置定时器, 例如:
     * ConfigCpuTimer(&CpuTimer0, 150, 16.666667);  // 150MHz, 16.67us周期
     */

    /* 应用层 GPIO + 外设初始化 (SCI-A ISR 注册 + PIE 使能在其内部完成) */
    AppConfig_Init();

    /* 5. 使能中断 */
    PieCtrlRegs.PIECTRL.bit.ENPIE = 1;  /* 使能 PIE 中断控制器 */
    EINT;                                /* 开全局中断 */
    ERTM;                                /* 开实时调试中断 */

    /* 6. 应用初始化 */
    /* TODO: 启动定时器、使能 PWM 输出等 */

    /* 7. 主循环 */
    for (;;)
    {
        /* 在此放置非实时任务:
         * - 状态机更新
         * - 通信协议处理 (Modbus, CAN, ...)
         * - 数据记录
         * - 保护逻辑检查
         */
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

/**
 * @brief SCI-A RX FIFO 中断服务例程 (桩)
 *
 * 当前为最小实现: 仅清除中断标志 + PIE 应答, 防止中断挂死
 * 完整实现将在 Step 2.3 中完成 (读取 FIFO + 回显)
 */
interrupt void ISRSciRx(void)
{
    /* 读取 RXBUF 清除 FIFO 中断标志 (即使不使用数据也要读) */
    Uint16 dummy = SciaRegs.SCIRXBUF.all;

    /* 清除 RX FIFO 中断标志 */
    SciaRegs.SCIFFRX.bit.RXFFINTCLR = 1;

    /* 清除 PIE Group 9 中断应答 */
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP9;
}
