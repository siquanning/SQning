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
/* 跨 ISR/主循环的共享变量, 使用 volatile 修饰 */

#define RX_BUF_SIZE  256                       /* 接收缓冲区大小 (字节) */

volatile Uint16 g_rxBuffer[RX_BUF_SIZE];        /* SCI 接收环形缓冲      */
volatile Uint16 g_rxHead     = 0;               /* 缓冲区写入位置         */
volatile Uint16 g_rxTail     = 0;               /* 缓冲区读取位置         */
volatile Uint16 g_rxReady    = 0;               /* 帧就绪标志: 1=有待发数据 */

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
    /* TODO: 启动定时器、使能 PWM 输出等 */

    /* 7. 主循环 */
    for (;;)
    {
        /* ---- SCI 回显: ISR 收数据 → 等帧间超时 → 整体回显 ---- */
        if (g_rxReady)
        {
            Uint16 prevHead;
            Uint16 idleCount = 0;

            /* 等待帧间静默: 连续 4ms 无新字节到达则认为帧结束 */
            do {
                prevHead = g_rxHead;
                DELAY_US(1000);              /* 等待 1ms                  */

                if (g_rxHead != prevHead)
                {
                    idleCount = 0;           /* 有新数据, 重置静默计时    */
                }
                else
                {
                    idleCount++;             /* 无新数据, 累计静默时间    */
                }
            } while (idleCount < 4);         /* 连续 4ms 静默 → 帧结束    */

            {
                Uint16 i;
                Uint16 count = g_rxHead;     /* 快照: 防止 ISR 在发送期间追加数据 */

                for (i = 0; i < count; i++)
                {
                    SciSendByte(g_rxBuffer[i]);
                }

                /* 闪 TX LED 表示完成一次回显 */
                CLEAR_TXLED;
                DELAY_US(1000);
                SET_TXLED;

                /* 从缓冲区移除已发送的数据 */
                g_rxHead -= count;
                if (g_rxHead > 0)
                {
                    /* ISR 在发送期间追加了新数据, 前移 */
                    Uint16 j;
                    for (j = 0; j < g_rxHead; j++)
                    {
                        g_rxBuffer[j] = g_rxBuffer[count + j];
                    }
                }
            }

            g_rxReady = (g_rxHead > 0) ? 1 : 0;
        }

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
 * @brief SCI-A RX FIFO 中断服务例程
 *
 * 触发条件: RX FIFO 中数据量 ≥ 触发深度 (1 字节)
 *
 * 处理逻辑:
 *   1. 循环读取 RX FIFO 中所有字节, 存入全局接收缓冲
 *   2. 收到数据后置帧就绪标志 g_rxReady
 *   3. 闪 RX LED 提示收到数据
 *   4. 缓冲满时丢弃并重新开始 (软件流控)
 *
 * 注意: 主循环负责将缓冲数据发回 (回显)
 */
interrupt void ISRSciRx(void)
{
    Uint16 rxData;

    /* 1. 先清除 RX FIFO 中断标志, 再读 FIFO (TI 推荐顺序) */
    SciaRegs.SCIFFRX.bit.RXFFINTCLR = 1;

    /* 2. 确认中断标志确实置位 (防御性检查) */
    if (SciaRegs.SCIFFRX.bit.RXFFINT == 1)
    {
        /* 3. 循环读取 RX FIFO 中所有待处理字节 */
        while (SciaRegs.SCIFFRX.bit.RXFFST > 0)
        {
            rxData = SciReceiveByte();

            /* 缓冲未满则存入, 满则丢弃整帧重新开始 */
            if (g_rxHead < RX_BUF_SIZE)
            {
                g_rxBuffer[g_rxHead++] = rxData;
            }
            else
            {
                /* 缓冲溢出: 清空, 重新接收 */
                g_rxHead = 0;
            }
        }

        /* 4. 有新数据则置标志, 通知主循环处理 */
        if (g_rxHead > 0)
        {
            g_rxReady = 1;
        }

        /* 5. 翻转 RX LED 提示收到数据 */
        TOGGLE_RXLED;
    }

    /* 6. 清除 PIE Group 9 中断应答 */
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP9;
}
