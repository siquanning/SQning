/**
 * @file    APP_CONFIG.h
 * @brief   应用层配置头文件 — 引脚定义, 常量, 公开接口
 *
 * 接口约定:
 *   - 所有宏定义引脚使用大写 + 描述性名称
 *   - 提供对外公开的函数原型
 *   - 时钟/频率相关的常量在此集中定义
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "DSP2833x_Device.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 系统时钟定义
 * ================================================================ */

/* 由 DSP2833x_Examples.h 中的 PLL 配置决定:
 *   DSP28_PLLCR = 10, DSP28_DIVSEL = 2  →  CPU_CLK = 150 MHz
 *   修改 PLL 配置时, 同步更新以下宏 */
#define CPU_CLK     150000000L  /* Hz, CPU 主频 (SYSCLKOUT)          */
#define LSPCLK       37500000L  /* Hz, 低速外设时钟 (SYSCLKOUT/4)   */
#define HSPCLK       75000000L  /* Hz, 高速外设时钟 (SYSCLKOUT/2)   */

/* ================================================================
 * PWM 配置
 * ================================================================ */

/* TODO: 根据你的开关频率修改 PWM_CLK */
#define PWM_CLK     60000       /* Hz, PWM 开关频率                  */
#define SP          (Uint16)(CPU_CLK / (2UL * PWM_CLK))  /* TBPRD 值 */

/* ================================================================
 * SCI 通信参数
 * ================================================================ */

#define SCI_BAUD       9600       /* bps, UART 波特率                */
/* BRR = LSPCLK / (Baud × 8) − 1 = 37500000 / 76800 − 1 = 487 */
#define SCI_BRR_VALUE   487       /* SCI 波特率除数 (0x1E7)          */

/* ================================================================
 * SPI 通信参数
 * ================================================================ */

/* SPI 波特率: LSPCLK / (SPIBRR + 1) = 37500000 / 128 ≈ 292.97 kHz
 * SPI 时钟速率远高于 9600bps UART, 有效吞吐量由主循环轮询周期控制
 * SPIBRR 最大值 127 (7 位) */
#define SPI_BRR_VALUE   127       /* SPI 波特率除数 (最慢=292.97kHz) */

/* ================================================================
 * 控制参数
 * ================================================================ */

/* TODO: 定义你的控制周期、滤波器参数等 */

/* ================================================================
 * GPIO 引脚定义
 * ================================================================ */

/* 使用宏封装 GPIO 操作, 便于阅读和维护
 *
 * 命名约定:
 *   SET_<PIN_NAME>    — 置高
 *   CLEAR_<PIN_NAME>  — 拉低
 *   READ_<PIN_NAME>   — 读取
 *
 * 示例:
 *   #define SET_LED       GpioDataRegs.GPASET.bit.GPIO5 = 1
 *   #define CLEAR_LED     GpioDataRegs.GPACLEAR.bit.GPIO5 = 1
 *   #define READ_LED      GpioDataRegs.GPADAT.bit.GPIO5
 *
 * 警告: GPIO SET/CLEAR/DAT 寄存器对不同的 PORT 使用不同的寄存器名:
 *   GPIO0-31   →  GPASET, GPACLEAR, GPADAT  (端口 A)
 *   GPIO32-63  →  GPBSET, GPBCLEAR, GPBDAT  (端口 B)
 *   GPIO64-87  →  GPCSET, GPCCLEAR, GPCDAT  (端口 C)
 */

/* ---- 用户 GPIO 定义 ---- */
/* TX LED: GPIO67 (GPIOC), 低电平点亮, 发送完成时闪烁 */
#define SET_TXLED    GpioDataRegs.GPCSET.bit.GPIO67 = 1    /* 高电平 → LED 灭 */
#define CLEAR_TXLED  GpioDataRegs.GPCCLEAR.bit.GPIO67 = 1  /* 低电平 → LED 亮 */
#define TOGGLE_TXLED GpioDataRegs.GPCTOGGLE.bit.GPIO67 = 1 /* 翻转 TX LED    */

/* RX LED: GPIO68 (GPIOC), 低电平点亮, 收到完整帧时闪烁 */
#define SET_RXLED    GpioDataRegs.GPCSET.bit.GPIO68 = 1    /* 高电平 → LED 灭 */
#define CLEAR_RXLED  GpioDataRegs.GPCCLEAR.bit.GPIO68 = 1  /* 低电平 → LED 亮 */
#define TOGGLE_RXLED GpioDataRegs.GPCTOGGLE.bit.GPIO68 = 1 /* 翻转 RX LED    */

/* SPI NSS: GPIO19 (GPIOA), 输出, 永久拉低选中 CPLD */
#define SPI_CS_LOW   GpioDataRegs.GPACLEAR.bit.GPIO19 = 1
#define SPI_CS_HIGH  GpioDataRegs.GPASET.bit.GPIO19 = 1

/* ================================================================
 * XINTF / 外部 ADC 地址定义 (按需)
 * ================================================================ */

/* 如果使用 XINTF Zone 7 连接外部设备, 在此定义基地址:
 * #define EXT_ADC_BASIC_A   (*((volatile Uint16 *)0x200001))
 * #define EXT_ADC_BASIC_B   (*((volatile Uint16 *)0x200002))
 */

/* ================================================================
 * 公开函数原型
 * ================================================================ */

void AppConfig_Init(void);          /* 应用层总初始化               */
void AppConfig_InitGpio(void);      /* GPIO 配置 (SCI 引脚 + LED)   */
void AppConfig_InitSci(void);       /* SCI-A 初始化 (9600bps+FIFO)  */
void AppConfig_InitSpi(void);       /* SPI-A 主机模式初始化         */
void AppConfig_InitCpuTimer0(void); /* CPU Timer0 1ms 时基+ISR     */

/* 中断服务例程 (在 MAIN.c 中实现) */
extern interrupt void ISRSciRx(void);
extern interrupt void ISRTimer0(void);

/* 全局变量 (在 MAIN.c 中定义) */
extern volatile Uint32 g_sysTick;   /* 系统滴答: ISRTimer0 每 1ms 递增 */

/* 字节级收发封装 */
void   SciSendByte(Uint16 data);
Uint16 SciReceiveByte(void);
void   SpiSendByte(Uint16 data);
Uint16 SpiReceiveByte(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H */
