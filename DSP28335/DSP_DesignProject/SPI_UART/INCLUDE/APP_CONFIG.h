/**
 * @file    APP_CONFIG.h
 * @brief   应用层配置头文件 — 引脚定义, 常量, 公开接口 (SPI_UART 透传桥)
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
#define CPU_CLK        150e6       /* Hz, CPU 主频 (SYSCLKOUT)              */
#define CPU_RATE_NS      6.667L   /* ns, 每个 CPU 周期的时间               */
#define LSPCLK        37.5e6      /* Hz, 低速外设时钟 (SYSCLKOUT/4, 默认)  */

/* ================================================================
 * UART (SCI-A) 配置
 * ================================================================ */

#define SCI_BAUD        9600       /* 波特率 (bit/s)                       */

/* ================================================================
 * SPI-A 配置
 * ================================================================ */

/* SPI 波特率 = LSPCLK / (SPI_BRR + 1)
 * SPI_BRR = 127 → SPI CLK ≈ 293 kHz (最慢速率, 有效吞吐由 1ms ISR 控制) */
#define SPI_BRR           127

/* ================================================================
 * CPU Timer0 配置
 * ================================================================ */

#define TIMER0_PERIOD_US 1000       /* us, Timer0 中断周期 (1ms 主循环)    */

/* ================================================================
 * 缓冲区配置
 * ================================================================ */

#define RX_BUF_SIZE       256       /* 字节, 接收环形缓冲区大小             */

/* ================================================================
 * LED 引脚定义 (GPIOC 端口)
 * ================================================================ */

/* TX LED — GPIO67 (GPIOC.3), 低电平点亮 */
#define SET_LED_TX     GpioDataRegs.GPCSET.bit.GPIO67 = 1
#define CLEAR_LED_TX   GpioDataRegs.GPCCLEAR.bit.GPIO67 = 1
#define TOGGLE_LED_TX  GpioDataRegs.GPCTOGGLE.bit.GPIO67 = 1

/* RX LED — GPIO68 (GPIOC.4), 低电平点亮 */
#define SET_LED_RX     GpioDataRegs.GPCSET.bit.GPIO68 = 1
#define CLEAR_LED_RX   GpioDataRegs.GPCCLEAR.bit.GPIO68 = 1
#define TOGGLE_LED_RX  GpioDataRegs.GPCTOGGLE.bit.GPIO68 = 1

/* ================================================================
 * 公开函数原型 — 驱动层初始化
 * ================================================================ */

void AppConfig_Init(void);          /* 应用层总初始化                     */
void AppConfig_InitGpio(void);      /* GPIO 配置 (SPI/SCI/LED)           */
void AppConfig_InitSci(void);       /* SCI-A UART 初始化                 */
void AppConfig_InitSpi(void);       /* SPI-A 主机初始化                  */
void AppConfig_InitCpuTimer0(void); /* CPU Timer0 1ms 时基               */

/* ================================================================
 * 公开函数原型 — 数据收发
 * ================================================================ */

Uint16 SciReceiveByte(void);        /* 从 SCI RX FIFO 读 1 字节, 空返回 0xFFFF */
void   SciSendByte(Uint16 byte);    /* 向 SCI TX FIFO 写 1 字节                 */
Uint16 SpiTransferByte(Uint16 txByte); /* SPI 全双工传输 1 字节, 返回接收值     */

/* ================================================================
 * 公开函数原型 — 帧缓冲管理
 * ================================================================ */

void   SciRxBuf_PushByte(Uint16 byte);  /* SCI 接收缓冲写入             */
Uint16 SciRxBuf_PopByte(void);          /* SCI 接收缓冲读取并移除       */
Uint16 SciRxBuf_IsFrameReady(void);     /* SCI 帧完整? (Modbus 3.5 超时) */

void   SpiRxBuf_PushByte(Uint16 byte);  /* SPI 接收缓冲写入              */
Uint16 SpiRxBuf_PopByte(void);          /* SPI 接收缓冲读取并移除        */
Uint16 SpiRxBuf_IsFrameReady(void);     /* SPI 帧完整? (Modbus 3.5 超时) */

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H */
