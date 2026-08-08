# 产品需求文档 (PRD) | NSQ

> **本文件由用户与 AI 在阶段1共同填写。AI 不得在 PRD 确认前编写任何代码。**

---

## 1. 项目信息

| 字段 | 内容 |
|---|---|
| 项目名称 | SPI_UART（SPI↔UART 双向透传桥） |
| 版本 | v0.1.0 |
| 目标平台 | TMS320F28335 (C2000 Delfino, 150MHz, FPU) |
| 开发工具 | Code Composer Studio, TI C2000 Compiler v6.2.7 |
| 创建日期 | 2026-08-08 |
| 最后更新 | 2026-08-08 |

---

## 2. 项目概述

本项目实现一个 SPI↔UART 双向透明传输桥。DSP 作为数据中转站：UART 侧连接 PC（Modbus RTU 协议），SPI 侧连接 CPLD，两端数据双向透传。DSP 不解析 Modbus 帧内容，仅负责完整帧的缓冲与转发；通过软件流控管理两端速率匹配。

---

## 3. 功能需求

### 3.1 核心功能

| 编号 | 功能 | 描述 | 优先级 |
|---|---|---|---|
| F-001 | UART 通信 | SCI 模块，9600 bps，8N1，接收 PC 下发的 Modbus RTU 帧并回传 CPLD 应答 | P0 |
| F-002 | SPI 通信 | SPI 模块，与 CPLD 双向数据交换，速率等效 9600 bps | P0 |
| F-003 | 双向透传 | UART RX → SPI TX（PC→CPLD），SPI RX → UART TX（CPLD→PC），全双工透明转发 | P0 |
| F-004 | 软件流控 | 双缓冲区管理：UART 接收缓冲 + SPI 接收缓冲，满时丢弃/覆盖旧数据，防止速率不匹配丢帧 | P0 |
| F-005 | Modbus 帧边界检测 | 基于 3.5 字符超时（≈4ms @9600）判定帧结束，收到完整帧后再转发，避免碎片化传输 | P0 |
| F-006 | 收发 LED 闪烁 | TX LED（GPIO5）：每次 UART 发送完成闪烁一次；RX LED（GPIO6）：每次收到完整帧闪烁一次 | P1 |

---

## 4. 性能指标

| 指标 | 目标值 | 单位 | 备注 |
|---|---|---|---|
| UART 波特率 | 9600 | bps | 8N1 |
| SPI 等效速率 | ~9600 | bps | 与 UART 速率匹配 |
| 主循环周期 | 1 | ms | CPU Timer0 中断驱动 |
| 帧间超时 | 4 | ms | Modbus 3.5 字符 @9600 |
| CPU 占用率 (平均) | < 10 | % | 低速率透传，负载极轻 |
| CPU 占用率 (峰值) | < 30 | % | |
| RAM 使用 | < 2 | KB | 双 256B 缓冲区 |
| Flash 使用 | < 8 | KB | |

---

## 5. 硬件接口

### 5.1 GPIO 分配

| GPIO | 方向 | 功能 | 备注 |
|---|---|---|---|
| 16 | 输出 | SPI-A SIMO (MOSI) | 接 CPLD SOMI |
| 17 | 输入 | SPI-A SOMI (MISO) | 接 CPLD SIMO |
| 18 | 输出 | SPI-A CLK | DSP 提供时钟（DSP 做主机） |
| 19 | 输出 | SPI-A STE (CS) | CPLD 片选，低有效 |
| 35 | 输出 | SCI-A TX | 接 PC RX（UART→USB），MUX=2 |
| 36 | 输入 | SCI-A RX | 接 PC TX（UART→USB），MUX=2 |
| 67 | 输出 | LED TX 指示 | 发送完成闪烁，低电平点亮，GPIOC |
| 68 | 输出 | LED RX 指示 | 接收完成闪烁，低电平点亮，GPIOC |

### 5.2 外设使用

| 外设 | 用途 | 配置参数 |
|---|---|---|
| SCI-A | UART 通信 (PC) | 9600 bps / 8N1 / 无流控 / FIFO 8 字节 / GPIO35(TX)+36(RX) MUX=2 |
| SPI-A | SPI 通信 (CPLD) | 主机模式 / 8-bit / 无相位滞后 / 下降沿输出 / 波特率≈9600 bps |
| CPU Timer0 | 主循环时基 + Modbus 帧超时检测 | 1 ms 周期 |

### 5.3 外部连接

| 接口 | 连接设备 | 协议/电平 | 备注 |
|---|---|---|---|
| SCI-A (GPIO35/36) | PC (USB转TTL) | UART 3.3V TTL / Modbus RTU | 交叉连接：RX↔TX，MUX=2 备选引脚 |
| SPI-A (GPIO16-19) | CPLD | SPI 3.3V / 透明帧数据 | CLK 由 DSP 提供，DSP 为主机 |
| GPIO67/68 | LED | 3.3V 推挽输出 | 低电平点亮，GPIOC 口，串 330Ω 限流 |

---

## 6. 控制算法

本项目为纯数据透传，不涉及控制算法。

---

## 7. 保护与安全

| 保护功能 | 触发条件 | 响应动作 | 响应时间 |
|---|---|---|---|
| 接收缓冲区溢出 | 缓冲满时新数据到达 | 丢弃当前缓冲区，重新开始帧接收 | 即时 |
| 帧超时异常 | 帧接收中途超过帧间超时 | 丢弃不完整帧，等待下一帧 | 4 ms |

---

## 8. 非功能需求

### 8.1 实时性
- 主循环周期: 1 ms（CPU Timer0）
- 帧转发延迟: < 10 ms（从接收完最后一字节到开始发送）

### 8.2 可靠性
- 看门狗: 禁用（透传应用，无需复位）
- 上电初始化自检: 不需要
- ECC/CRC 校验: 透传模式，CRC 由端设备（PC/CPLD）负责

### 8.3 可维护性
- 调试串口输出: 不需要
- LED 状态指示: 需要（GPIO5 TX 闪烁 / GPIO6 RX 闪烁）

---

## 9. 约束条件

- 编译器必须支持 `--float_support=fpu32`
- 代码需在 RAM (Debug) 和 Flash (Release) 两种模式下均可运行
- SPI 数据帧格式与 UART 数据帧格式一致（8-bit 透明字节流，Modbus RTU）
- DSP 不解析 Modbus 帧内容，仅检测帧边界用于完整帧转发
- 双向全双工：UART→SPI 和 SPI→UART 两条路径可同时工作

---

## 10. 参考资料

- [TMS320F28335 数据手册](https://www.ti.com/lit/ds/symlink/tms320f28335.pdf)
- [DSP2833x Header Files V1.01](INCLUDE/)
- [Modbus RTU 协议规范](https://modbus.org/docs/Modbus_Application_Protocol_V1_1b3.pdf)

---

*本文件由 AI 与用户协作填写。填写完成后请用户确认，然后进入阶段2(生成实施计划)。*

---

*Template by NSQ*
