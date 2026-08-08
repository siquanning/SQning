# 学习笔记 (Learning Notes) | NSQ

> 这是"跟着 AI 学 DSP"的**课后笔记本**：AI 每完成一个 Step，把新学到的知识点按主题追加到这里。
> 与 `docs/DSP_LEARNING_GUIDE.md` 的区别：指南是项目规划（课前预习），笔记是每步积累（课后沉淀）。
> 笔记必须用大白话，让不懂 DSP 的人也能看懂。每完成一个 Step，若有新知识点就追加到对应主题下，并标注日期。

---

## 主题索引

- [1. 工程结构与编译流程](#1)
- [2. main() 启动流程](#2)
- [3. 时钟系统](#3)
- [4. GPIO](#4)
- [5. ePWM](#5)
- [6. ADC](#6)
- [7. 中断与定时器](#7)
- [8. 串口/通信](#8)
- [9. 调试技巧](#9)
- [10. 其他](#10)

---

## 1. 工程结构与编译流程
*(2026-08-08, Step 1.1)*

### 工程里都有什么文件夹？
- `SRC/` — 放 C 源码。其中 `DSP2833x_*.c` 是 TI 官方库（不要改），`MAIN.c` 和 `APP_CONFIG.c` 是我们写代码的地方
- `INCLUDE/` — 放 .h 头文件。`DSP2833x_*.h` 是 TI 的寄存器定义（每个寄存器都有 .bit.xxx 可读写的位域），`APP_CONFIG.h` 是我们自己定义的宏和函数声明
- `CMD/` — 链接器命令文件（.cmd），告诉链接器"程序放哪里、数据放哪里"
- `Debug/` — 编译输出目录（.obj、.out、.map 都在这里）
- `docs/` — 学习参考文档

### .cmd 文件是干嘛的？
DSP 的内存是分块的（M0/M1/L0-L7/Flash），.cmd 文件就是"内存分配表"：
- `28335_RAM_lnk.cmd` — Debug 用的，程序和数据都放 RAM（掉电丢失，但下载快）
- `CMD/F28335.cmd` — Release 用的，程序放 Flash（掉电不丢），数据放 RAM
- `CMD/DSP2833x_Headers_nonBIOS.cmd` — 把外设寄存器（SciaRegs 等）映射到对应的内存地址

### 编译链接大概发生了什么？
1. 编译器把每个 .c 编译成 .obj（机器码 + 符号表）
2. 链接器按 .cmd 文件的指示，把 .obj 拼到一起，给每个函数/变量分配地址
3. 输出 .out 文件（可执行文件），下载到 DSP 的 RAM/Flash 里

## 2. main() 启动流程
*(2026-08-08, Step 1.1)*

### main() 初始化为什么是这个顺序？
```
InitSysCtrl() → DINT → InitPieCtrl() → InitPieVectTable() → 注册ISR → 外设初始化 → EINT
```
1. **InitSysCtrl()** — 配时钟（PLL 150MHz）+ 关看门狗 + 开外设时钟。任何事之前必须先把时钟搞好
2. **DINT** — 关全局中断。初始化中途不能让中断捣乱（比如配了一半的寄存器被 ISR 改了）
3. **InitPieCtrl() + InitPieVectTable()** — 把 PIE 中断控制器复位 + 把向量表填上默认的"空 ISR"
4. **注册 ISR** — 把我们的 ISR 函数地址写进向量表对应位置（必须包在 EALLOW/EDIS 里，因为向量表是受保护寄存器）
5. **外设初始化** — GPIO → SCI → SPI → Timer，按依赖顺序
6. **EINT** — 所有东西都配好了，开中断，开始实时工作

## 3. 时钟系统
*(2026-08-08, Step 1.1)*

### 150MHz 怎么来的？
`InitSysCtrl()` 里配了 PLL：晶振 30MHz → PLL × 10 = 300MHz → DIVSEL / 2 = **150MHz** (SYSCLKOUT)

### 为什么还有一个 LSPCLK = 37.5MHz？
SYSCLKOUT 太快了（150MHz），低速外设（SCI/SPI/eCAN）跑不了那么快。所以芯片会自动做一次分频：
- **LSPCLK = SYSCLKOUT / 4 = 37.5MHz**（默认值，可通过 LOSPCP 寄存器改）

SCI 和 SPI 的波特率都是从 LSPCLK 再分出来的。

### 关看门狗是什么意思？
看门狗是一个倒计时器，如果程序不按时"喂狗"（复位计时器），它就会强制复位 CPU。开发阶段通常关掉，省得调试时不小心复位。

## 4. GPIO
*(2026-08-08, Step 1.2)*

### 引脚的三个关键寄存器
每个 GPIO 引脚有三个关键配置：
| 寄存器 | 作用 | 例子 |
|---|---|---|
| MUX (复用选择) | 决定引脚是 GPIO 还是外设功能 | `GPAMUX2.bit.GPIO16 = 3` → SPI 模式 |
| DIR (方向) | 输入(0) 还是 输出(1) | `GPADIR.bit.GPIO16 = 1` → 输出 |
| DAT/SET/CLEAR/TOGGLE | 读写引脚电平 | `GPASET.bit.GPIO5 = 1` → 拉高 |

### SET/CLEAR 为什么比直接写 DAT 好？
```c
GpioDataRegs.GPASET.bit.GPIO5 = 1;   // 只把 GPIO5 拉高
GpioDataRegs.GPADAT.bit.GPIO5 = 1;   // 读-改-写整个端口, 可能误改其他引脚
```
SET/CLEAR 是**原子操作**：硬件直接操作单个引脚，不会干扰同端口其他引脚。DAT 是读-改-写，中间如果有 ISR 同时改了另一个引脚，会被覆盖回去。

### QSEL 和 PUD 是干什么的？
- **QSEL** (输入量化选择)：决定引脚的输入信号是否经过同步触发器。QSEL=3（异步）旁路了同步触发器，信号直接进模块，延迟最小但不滤毛刺。UART 通信用异步模式够用
- **PUD** (内部上拉)：PUD=0 时内部上拉电阻生效，引脚悬空时默认高电平，防止噪声误触发

## 5. ePWM
*(AI 填写：时基/比较/动作/死区/故障保护，占空比怎么算)*

## 6. ADC
*(AI 填写：采样触发、排序器、结果读取)*

## 7. 中断与定时器
*(AI 填写：PIE 分组、向量注册、ISR 规范、PIEACK)*

## 8. 串口/通信
*(2026-08-08, Step 2.1)*

### SCI 是什么？
SCI（Serial Communication Interface）是 TI 对 UART 的叫法，本质就是大家熟知的"串口"。F28335 有 3 个 SCI 模块（SCI-A / SCI-B / SCI-C）。

### SCI 配置的关键寄存器

| 寄存器 | 做什么 | 本工程的值 |
|---|---|---|
| SCICCR | 通信格式：几位数据、有无校验、几个停止位 | 0x0007 = 1停止/无校验/8数据 |
| SCIHBAUD + SCILBAUD | 波特率（16 位，分两个寄存器） | 487 (BRR) |
| SCICTL1 | TX/RX 使能 + SWRESET 复位控制 | 0x0023 (退出复位后) |
| SCIFFTX | TX FIFO 使能和复位 | 0xE040 (使能 FIFO + 复位) |
| SCIFFRX | RX FIFO 中断触发深度 | 0x0001 (无中断) |
| SCIFFCT | 自动波特率检测等 | 0x0000 |

### 波特率怎么算？
```
波特率 = LSPCLK / [(BRR + 1) × 8]
     BRR = LSPCLK / (目标波特率 × 8) - 1
```
**关键理解**：除以 8 是因为 SCI 内部用 16× 波特率的时钟来采样，每 bit 采 16 次，后来简化为 8×。每个 bit 取第 8/9/10 三个采样点的多数投票，能过滤掉毛刺。

本工程：BRR = 37,500,000 / (9600 × 8) - 1 ≈ **487**，实际波特率 ≈ 9605.5 bps，误差 +0.06%。

### SWRESET 机制（SCI 的特色设计）
SCI 很多寄存器必须 SWRESET=0 时才能改（就像"配置模式"），改完了再写 SWRESET=1 让硬件真正开始跑。切换方法：
1. 先写 `SCICTL1=0x0003`（SWRESET=0，TX/RX 使能位可写、FIFO 可配置）
2. 配完所有寄存器
3. 再写 `SCICTL1=0x0023`（SWRESET=1，SCI 真正开始收发）

### 中断 vs 轮询两种接收方式
| 方式 | 怎么用 | 优点 | 缺点 |
|---|---|---|---|
| 中断接收 | 使能 RX FIFO 中断 (SCIFFRX.RXFFIENA=1)，每收够 N 字节触发 ISR | 响应快，不浪费 CPU | 有中断优先级冲突风险 |
| 轮询接收 | ISR/Timer 里定期读 `SCIFFRX.RXFFST` (FIFO 里有多少字节) | 简单，无中断冲突 | 有轮询延迟（最多 1ms） |

本工程选**轮询方式**：9600bps ≈ 每秒 960 字节 = 约 1ms 来 1 字节。Timer0 ISR 每 1ms 查一次 FIFO，能及时取走数据而不溢出（FIFO 有 16 字节深度）。

### RXFFST 是什么？
`SCIFFRX.bit.RXFFST` 是一个 5 位字段，告诉你 RX FIFO 里现在有几个字节等着读。轮询模式就靠它：`if (SciaRegs.SCIFFRX.bit.RXFFST > 0) { byte = SciaRegs.SCIRXBUF.all; }`

## 9. 调试技巧
*(AI 与用户调试中共同积累：怎么看寄存器、怎么设断点、怎么用 Expressions 窗口、常见故障排查)*

## 10. 其他
*(放不进去的知识点，如 IQMath、Flash 烧写、内存布局)*

---

*维护规则：每完成一个 Step，若产生新知识点，追加到对应主题下并标注日期。*

---

*Template by NSQ*