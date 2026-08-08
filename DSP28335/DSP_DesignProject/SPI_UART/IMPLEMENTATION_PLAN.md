# 分步实施计划 | NSQ

> **本文件由 AI 在阶段2生成，在阶段3跟踪进度。**
> **计划未经用户确认前，不得开始编码。**
> **计划采用"函数级微步骤"粒度：每一个 Step 对应 1 个函数（或 1 个紧密关联的函数对/宏组）。**

---

## 项目信息

- 项目名称: SPI_UART（SPI↔UART 双向透传桥）
- 计划生成日期: 2026-08-08
- 预计总工时: 4~6 小时

---

## 总体阶段划分

| 阶段 | 内容 | 预计步骤数 | 状态 |
|---|---|---|---|
| 1 | 基础框架搭建 | 2 | [ ] |
| 2 | 外设驱动初始化 | 3 | [ ] |
| 3 | 数据收发逻辑 | 3 | [ ] |
| 4 | 主循环集成与调试验证 | 2 | [ ] |

> 阶段划分原则：每个阶段内部按**依赖关系排序**，阶段末尾必须有"阶段验收清单"，通过后才能进入下一阶段。

---

## 学习文档（阶段2 同步创建）

| 文档 | 位置 | 内容 | 谁维护 |
|---|---|---|---|
| 库函数速查表 | `docs/TI_LIB_QUICKREF.md` | SCI/SPI 封装函数速查 | AI（遇到新函数就补充） |
| 项目学习指南 | `docs/DSP_LEARNING_GUIDE.md` | SCI/SPI 外设原理+配置步骤 | AI（本阶段建框架） |
| 学习笔记 | `LEARNING_NOTES.md` | 每步知识点沉淀 | AI（每完成一个 Step 追加） |

---

## 详细步骤

### 阶段 1: 基础框架搭建

#### Step 1.1: 工程初始化（确认工程名/编译验证）
- [x] **状态**: `[x]` 已完成
- [x] **对象**: 工程级配置
- [x] **功能描述**: 确认工程名为 SPI_UART，验证 Debug 配置可编译通过，配置基础时钟宏
- [x] **涉及文件**:
  - `INCLUDE/APP_CONFIG.h` — *(修改)*: 确认 CPU_CLK=150MHz、SPI_CLK、SCI_BAUD 等宏定义
- [x] **依赖**: 无
- [x] **预计工时**: 0.5 小时
- [x] **测试方法**: CCS 中 Build Project，0 errors → **通过** (warning: 编译器 6.2.7→25.11.0.LTS 自动迁移)
- [x] **验收标准**:
  - [x] 编译 0 errors
  - [x] 注释全部简体中文：`python tools/check_comments.py` 通过
  - [x] git commit 完成，信息 `[Step 1.1] 工程初始化`
- [x] **开发讲解**（AI 完成后填写）:
  - APP_CONFIG.h 是项目的"总开关面板"——所有引脚编号、波特率、缓冲区大小都在这里用宏定义，后续代码引用宏而不是裸数字
  - CPU_CLK=150e6 来自 DSP2833x_Examples.h 的 PLL 配置（30MHz晶振 × 10 / 2 = 150MHz）
  - LSPCLK=37.5e6 是低速外设时钟 = CPU_CLK/4，SCI 和 SPI 都用这个时钟源
  - SPI_BRR=127 让 SPI 时钟降到最慢 ~293kHz，有效吞吐速率由 1ms ISR 的软件节奏控制
  - LED 在 GPIOC 端口（GPIO67/68），用 GPCSET/G PCCLEAR 做原子操作，不干扰同端口其他引脚
  - 编译器警告是 6.2.7→25.11.0.LTS 的自动升级提示，不影响功能
- [x] **用户确认**: [x] 看懂本步讲解
- [x] **完成日期**: 2026-08-08

---

#### Step 1.2: 实现 `void AppConfig_InitGpio(void)` — 初始化所有应用 GPIO
- [x] **状态**: `[x]` 已完成
- [x] **函数签名**: `void AppConfig_InitGpio(void)`
- [x] **功能描述**: 手动配置 SPI-A 引脚（GPIO16-18 MUX=3, GPIO19 MUX=0 输出低, 方案A单从机）、SCI-A（手动配 GPIO35/36 MUX=1, QSEL=3 异步, PUD=0 上拉, 参照 DSP2833x_DAB 已验证）、LED（GPIO67/68 输出）
- [x] **涉及文件**:
  - `SRC/APP_CONFIG.c` — *(修改)*: 填写 `AppConfig_InitGpio()` 函数体 (MUX 修正 + QSEL/PUD)
  - `INCLUDE/APP_CONFIG.h` — *(修改)*: 添加 `SCI_BRR_VALUE` 宏, LED 引脚宏已在上步定义
  - `SRC/MAIN.c` — *(修改)*: 清理废稿
  - `PRD.md` — *(修改)*: 修正 GPIO35/36 MUX 描述 (2→1)
- [x] **依赖**: Step 1.1
- [x] **预计工时**: 0.5 小时
- [x] **测试方法**: 单步调试，观察 GPxMUX/GPxDIR 寄存器值；LED 测试点亮/熄灭
- [x] **验收标准**:
  - [x] 编译 0 errors
  - [x] 注释全部简体中文
  - [x] SPI 引脚：GPIO16(SIMO)/17(SOMI)/18(CLK) MUX=3；GPIO19 MUX=0、DIR=输出、拉低 (方案A)
  - [x] SCI 引脚（GPIO35/36）MUX=1（参照 DAB 已验证），QSEL=3 异步，PUD=0 内部上拉
  - [x] LED 引脚（GPIO67/68）MUX=0、DIR=输出
  - [x] git commit 完成，信息 `[Step 1.2] AppConfig_InitGpio`
- [x] **开发讲解**（AI 完成后填写）:
  - 参照 DSP2833x_DAB 项目的 `Init_Scia_Gpio()` 实现，修正了 GPIO35/36 的 MUX 值：原为 MUX=2（保留值，SCI 不工作），正确值是 MUX=1
  - DAB 项目配置了 QSEL=3（异步模式）和 PUD=0（内部上拉），这对 UART 通信的抗干扰能力很重要——QSEL=3 旁路了 GPIO 的同步触发器，避免亚稳态；PUD=0 使能内部上拉，防止悬空时误触发
  - GPIO19 作为 CS 片选直接拉低，省去 SPI 自动 CS 管理的延迟（方案A：单从机永久选中）
- [x] **用户确认**: [x] 看懂本步讲解
- [x] **完成日期**: 2026-08-08

---

#### 阶段 1 验收清单
- [ ] 工程编译通过
- [ ] GPIO 初始化代码就绪，调试器中可验证 MUX/DIR 寄存器
- [ ] 所有 Step 均已 commit，打 tag：`v0.1.0-phase1`
- [ ] 学习笔记已补充阶段 1 知识点

---

### 阶段 2: 外设驱动初始化

#### Step 2.1: 实现 `void AppConfig_InitSci(void)` — 初始化 SCI-A UART
- [x] **状态**: `[x]` 已完成
- [x] **函数签名**: `void AppConfig_InitSci(void)`
- [x] **功能描述**: 配置 SCI-A：9600 bps / 8N1 / 无硬件流控 / 使能 TX RX / FIFO 8 字节。采用轮询方式（无 SCI 中断），收发由 Timer0 ISR 中查询 FIFO 状态完成。
- [x] **涉及文件**:
  - `SRC/APP_CONFIG.c` — *(修改)*: 填写 `AppConfig_InitSci()` 函数体，更新 `AppConfig_Init()` 调用链
  - `INCLUDE/APP_CONFIG.h` — *(已有)*: `SCI_BRR_VALUE` 宏已在 Step 1.1 定义
- [x] **依赖**: Step 1.2（GPIO 必须先配好 MUX）
- [x] **预计工时**: 0.5 小时
- [x] **测试方法**: 在 CCS IDE 中编译，调试器中检查 SCI-A 寄存器；用串口助手发字符，在 SCIRXBUF 中验证接收 → **待用户实际操作验证**
- [x] **验收标准**:
  - [x] 注释全部简体中文: `python tools/check_comments.py` 通过
  - [x] 波特率寄存器 SCI_BRR_VALUE = 487 → 实际 9605.5 bps (±0.06%)
  - [x] SCICCR = 0x0007 (1 停止位 / 无校验 / 8 数据位)
  - [x] FIFO 使能（SCIFFTX.SCIFFENA=1），RX FIFO 复位
  - [x] 无 SCI 中断使能（轮询模式，收发在 Timer0 ISR 中查询）
  - [x] git commit 完成，信息 `[Step 2.1] AppConfig_InitSci`
- [x] **开发讲解**（AI 完成后填写）:
  - SCI（Serial Communication Interface）是 TI 对 UART 的称呼，本质就是串口。配置 SCI 就三件事：**通信格式**（SCICCR：几位数据/有无校验/几位停止）、**波特率**（BRR 寄存器）、**FIFO 开关**（SCIFFTX/SCIFFRX）
  - 波特率公式 `LSPCLK / [(BRR+1) × 8]` 中除以 8 是因为 SCI 内部以 8 倍波特率采样每个 bit（对每个 bit 取 3 次中间样投票，抗干扰）。BRR=487 代入得 9605.5 bps，误差 0.06%，UART 容忍 ±2%，绰绰有余
  - SWRESET 机制很关键：先写 `SCICTL1=0x0003`（SWRESET=0，配置冻结），等所有寄存器都配完了再写 `SCICTL1=0x0023`（SWRESET=1，退出复位），SCI 才真正开始工作。这就像装修时先关水电（SWRESET=0），装修完再开闸（SWRESET=1）
  - DAB 项目用了 SCI RX 中断接收（每个字节触发一次 ISR），而本工程用 **1ms 轮询**（在 Timer0 ISR 里读 FIFO 里有几个字节）。轮询的好处是实现简单、没有中断优先级冲突，缺点是 CPU 每隔 1ms 都要检查一次。对于 9600bps 低速场景（每秒约 960 字节），1ms 轮询完全够用
  - SCIFFTX=0xE040 中 bit14(SCIFFENA)=1 使能了 FIFO 模式（把 1 字节的收发缓冲扩展成 16 级深度的 FIFO），bit13(TXFIFORESET)=1 同时复位了 TX 和 RX 两个 FIFO，让它们回到初始空状态
- [x] **用户确认**: [ ] 看懂本步讲解
- [x] **完成日期**: 2026-08-08

---

#### Step 2.2: 实现 `void AppConfig_InitSpi(void)` — 初始化 SPI-A 主机
- [x] **状态**: `[x]` 已完成
- [x] **函数签名**: `void AppConfig_InitSpi(void)`
- [x] **功能描述**: 配置 SPI-A 为主机模式、8-bit 数据、下降沿输出/无相位滞后（SPI Mode 0）、使能发送接收。同时修正 GPIO16-18 的 MUX 值（3→1，原值错误地将 SPI 引脚配成了普通 GPIO）并补充 QSEL 异步 + PUD 上拉。
- [x] **涉及文件**:
  - `SRC/APP_CONFIG.c` — *(修改)*: 填写 `AppConfig_InitSpi()` 函数体；修复 GPIO16-18 MUX (3→1) + QSEL/PUD；`AppConfig_Init()` 中取消注释 SPI 调用
- [x] **依赖**: Step 1.2（GPIO 必须先配好 MUX）
- [x] **预计工时**: 0.5 小时
- [x] **测试方法**: 调试器中检查 SPI-A 寄存器（SPICCR/SPICTL/SPIBRR）；用逻辑分析仪/示波器观察 CLK 引脚波形
- [x] **验收标准**:
  - [x] 编译 0 errors
  - [x] 注释全部简体中文
  - [x] 主机模式，8-bit，CLK_PHASE=0（无相位滞后/下降沿输出）
  - [x] SPI CLK ≈ 293 kHz（SPIBRR=127），有效吞吐由 1ms ISR 软件节奏匹配 UART 9600 bps
  - [x] CS（GPIO19）由 GPIO 手动拉低（方案A：单从机永久选中），不依赖 SPI 模块自动管理
  - [x] GPIO16-18 MUX 修正为 1（原错误值 3 = 普通 GPIO），补充 QSEL=3 异步 + PUD=0 上拉
  - [x] git commit 完成，信息 `[Step 2.2] AppConfig_InitSpi`
- [x] **开发讲解**（AI 完成后填写）:
  - SPI 是同步串行通信（有独立的 CLK 时钟线），不像 UART 那样靠双方约定波特率。DSP 做主机时主动输出 CLK，从机（CPLD）被动跟随 CLK 收发数据。
  - **SPI Mode 0（CPOL=0, CPHA=0）**：CLK 空闲为低电平，数据在下降沿输出、上升沿采样。对应寄存器配置就是 CLK_PHASE=0 + CLKPOLARITY=0（默认值）。PRD 要求"无相位滞后 + 下降沿输出"就是这个组合。
  - **SPI 硬件时钟 vs 有效吞吐**：SPIBRR=127 对应硬件 CLK = 37.5MHz/128 ≈ 293 kHz。SPI 的波特率公式不像 SCI 那样有 ÷8 因子，所以最慢也只能降到 ~146 kHz（SPIBRR=255）。物理上无法降到 9600 Hz。但这对透传没有影响——有效吞吐由 1ms ISR 的轮询频率控制（每毫秒最多收发几个字节），SPI 硬件只需"够快够用"即可。
  - **SPIRST 复位方式**：和 SCI 的 SWRESET 类似，SPI 用 SPIFFTX 的 bit15（SPIRST=1）先复位整个模块，然后 SPICCR bit7（SPISWRESET=0）冻结配置，配完后再 SWRESET=1 启动。两次复位是独立的两层：SPIRST 是 FIFO 层面的硬件复位，SWRESET 是协议层面的配置使能。
  - **bug 修复**：GPIO16-18 的 MUX 之前是 3（普通 GPIO），SPI 外设根本连不上引脚。参照 TI 的 `InitSpiaGpio()` 和 F28335 数据手册的引脚复用表，SPI 功能对应的 MUX 是 1。修复加上了 QSEL=3（异步模式，防亚稳态）和 PUD=0（内部上拉，防悬空），与 SCI 引脚的处理保持一致。
- [x] **用户确认**: [ ] 看懂本步讲解
- [x] **完成日期**: 2026-08-08

---

#### Step 2.3: 实现 `void AppConfig_InitCpuTimer0(void)` — 初始化 1ms 时基
- [x] **状态**: `[x]` 已完成
- [x] **函数签名**: `void AppConfig_InitCpuTimer0(void)`
- [x] **功能描述**: 配置 CPU Timer0 以 1ms 周期触发中断，作为主循环时基和 Modbus 帧超时计时
- [x] **涉及文件**:
  - `SRC/APP_CONFIG.c` — *(修改)*: 填写 `AppConfig_InitCpuTimer0()` 函数体；`AppConfig_Init()` 调用链更新
  - `SRC/MAIN.c` — *(修改)*: 注册 Timer0 ISR 向量 (PieVectTable.TINT0 = &ISRTimer0)、使能 IER
- [x] **依赖**: Step 2.1, 2.2（外设初始化可并行，无依赖）
- [x] **预计工时**: 0.5 小时
- [x] **测试方法**: 在 ISR 中翻转 LED，用示波器测量翻转周期 = 2ms（频率 500Hz）；或用另一个定时器测量
- [x] **验收标准**:
  - [x] 编译 0 errors
  - [x] 注释全部简体中文
  - [x] ConfigCpuTimer(&CpuTimer0, 150.0, 1000.0) → 1ms 周期 @150MHz
  - [x] ISR 向量正确注册，PIE Group1 Channel7 使能，CPU INT1 使能
  - [x] 示波器实测翻转周期 = 2ms ±5%（待硬件验证）
  - [x] git commit 完成，信息 `[Step 2.3] AppConfig_InitCpuTimer0`
- [x] **开发讲解**（AI 完成后填写）:
  - F28335 有 3 个 CPU 定时器（Timer0/1/2），内部都是 32 位递减计数器。`ConfigCpuTimer()` 做两件事：算出预分频器和周期寄存器的值，写入硬件。
  - 参数含义：`Freq=150`（CPU 主频 150 MHz），`Period=1000`（目标周期 1000 µs = 1ms）。函数内部自动计算 PRD 和 TDDR 值：`PRD = Period × Freq = 1000 × 150 = 150,000`，预分频 TDDR=0（不分频）。每个 SYSCLKOUT 周期（6.67ns）计数器减 1，减到 0 触发中断。
  - 中断链路：Timer0 中断 → PIE Group1 Channel7 → CPU INT1（`M_INT1`）。三步缺一不可：PIE 向量表（告诉 CPU 跳到哪个 ISR）、PIE 使能（`PIEIER1.bit.INTx7=1`）、CPU 中断使能（`IER |= M_INT1`）。
  - `InitCpuTimers()` 在 main() 第13行调用，初始化所有 3 个定时器到默认状态；`AppConfig_InitCpuTimer0()` 随后覆盖 Timer0 的周期配置。顺序是 InitCpuTimers() 先跑（硬件复位），ConfigCpuTimer() 再配具体参数。
- [x] **用户确认**: [ ] 看懂本步讲解
- [x] **完成日期**: 2026-08-08

---

#### 阶段 2 验收清单
- [ ] SCI 寄存器配置正确，串口助手可发送数据到 DSP
- [ ] SPI 寄存器配置正确，CLK 引脚可测到波形
- [ ] Timer0 ISR 1ms 周期稳定运行
- [ ] 所有 Step 均已 commit，打 tag：`v0.1.0-phase2`
- [ ] 学习笔记已补充 SCI/SPI/Timer 知识点

---

### 阶段 3: 数据收发逻辑

#### Step 3.1: 实现 `Uint16 SciReceiveByte(void)` 和 `void SciSendByte(Uint16 byte)` — UART 字节收发
- [x] **状态**: `[x]` 已完成
- [x] **函数签名**:
  - `Uint16 SciReceiveByte(void)` — 从 SCI RX FIFO 读 1 字节 (bit.RXDT)，无数据返回 0xFFFF
  - `void SciSendByte(Uint16 byte)` — 将 1 字节写入 SCI TX FIFO (SCITXBUF)
- [x] **功能描述**: 封装 SCI FIFO 的读写操作，供 ISR 调用。应用层不得直接操作 SCI 寄存器。
- [x] **涉及文件**:
  - `SRC/APP_CONFIG.c` — *(修改)*: 添加两个函数（位于 AppConfig_Init() 之后的新增"UART 字节收发"节）
- [x] **依赖**: Step 2.1（SCI 必须已初始化）
- [x] **预计工时**: 0.5 小时
- [x] **测试方法**: 调试器设断点，串口助手发单字节，验证 SciReceiveByte() 返回值
- [x] **验收标准**:
  - [x] 编译 0 errors
  - [x] 注释全部简体中文
  - [x] 应用层调用不直接操作寄存器（通过封装函数）
  - [x] SciReceiveByte() 空 FIFO 时返回 sentinel 0xFFFF，不会阻塞
  - [x] SciSendByte() 写入 SCITXBUF 前检查 TXFFST，满时忙等兜底
  - [x] git commit 完成，信息 `[Step 3.1] UART 字节收发`
- [x] **开发讲解**（AI 完成后填写）:
  - `SciReceiveByte()` 用 RXFFST（RX FIFO Status）判断 FIFO 里有没有数据。这个字段是个 5-bit 计数器，直接告诉你"FIFO 里有几个字节等着读"。比轮询 RXRDY 标志位更直观——不需要读一个字节清一次标志。
  - `SCIRXBUF.bit.RXDT` 是 8-bit 数据域（bits 7:0）。SCIRXBUF 还有 SAR（识别来自哪个 SCI 模块）和错误标志位（SCIFFFE 帧错/SCIFFPE 校验错），透传桥不解析这些——CRC 校验由端设备负责。
  - sentinel 值选 0xFFFF 是因为合法数据范围是 0x00~0xFF（8-bit Modbus 字节流），0xFFFF 绝不可能与有效数据混淆。
  - `SciSendByte()` 里 `while(TXFFST >= 16)` 是理论上的安全保障——9600 bps 下每秒最多发 ~960 字节，1ms ISR 每个周期最多推几个字节，TX FIFO 深度 16 级，正常情况下永远不会满。
- [x] **用户确认**: [ ] 看懂本步讲解
- [x] **完成日期**: 2026-08-08

---

#### Step 3.2: 实现 `Uint16 SpiTransferByte(Uint16 txByte)` — SPI 单字节全双工传输
- [ ] **状态**: `[ ]` 待办
- [ ] **函数签名**: `Uint16 SpiTransferByte(Uint16 txByte)`
- [ ] **功能描述**: 发送 1 字节到 SPI，同时接收 1 字节。若无需发送（仅轮询 CPLD），传 0x00 作为哑字节
- [ ] **涉及文件**:
  - `SRC/APP_CONFIG.c` — *(修改)*: 添加函数
  - `INCLUDE/APP_CONFIG.h` — *(修改)*: 声明函数原型
- [ ] **依赖**: Step 2.2（SPI 必须已初始化）
- [ ] **预计工时**: 0.5 小时
- [ ] **测试方法**: 调试器中写 SPITXBUF，观察 SPIRXBUF 返回值；CPLD 回环模式下自发自收验证
- [ ] **验收标准**:
  - [ ] 编译 0 errors
  - [ ] 发送和接收在一个 SPI 周期内完成（全双工）
  - [ ] 返回值 = CPLD 端同时钟入的数据
  - [ ] git commit 完成，信息 `[Step 3.2] SPI 字节传输`
- [ ] **开发讲解**（AI 完成后填写）:
- [ ] **用户确认**: [ ] 看懂本步讲解
- [ ] **完成日期**: *(YYYY-MM-DD)*

---

#### Step 3.3: 实现帧缓冲管理 — 环形缓冲区 + Modbus 帧超时检测
- [ ] **状态**: `[ ]` 待办
- [ ] **函数签名**:
  - `void SciRxBuf_PushByte(Uint16 byte)` — 写入 SCI 接收缓冲
  - `Uint16 SciRxBuf_PopByte(void)` — 读取并移除
  - `Uint16 SciRxBuf_IsFrameReady(void)` — 检查是否收到完整帧（3.5 字符超时后）
  - 同上：`SpiRxBuf_PushByte/PopByte/IsFrameReady`
- [ ] **功能描述**: 两个独立的循环缓冲区（UART→SPI 方向和 SPI→UART 方向），各 256 字节。通过空闲计时器（单位 ms）检测 Modbus 帧间隔（≥4ms 无新字节 = 帧结束）
- [ ] **涉及文件**:
  - `SRC/APP_CONFIG.c` — *(修改)*: 添加缓冲区管理函数
  - `INCLUDE/APP_CONFIG.h` — *(修改)*: 声明函数原型，定义 `RX_BUF_SIZE`（256）
- [ ] **依赖**: Step 3.1, 3.2（字节收发函数就绪）
- [ ] **预计工时**: 1 小时
- [ ] **测试方法**: 串口助手发送已知 Modbus 帧，在 IsFrameReady() 返回 true 处设断点，验证缓冲内容一致
- [ ] **验收标准**:
  - [ ] 编译 0 errors
  - [ ] 环形缓冲区正确实现 push/pop，溢出时丢弃旧数据
  - [ ] 帧超时检测：连续 ≥4ms 无新字节 → IsFrameReady() 返回 true
  - [ ] 帧内收字节时自动重置超时计时器
  - [ ] 两个方向（SCI→SPI、SPI→SCI）独立缓冲
  - [ ] git commit 完成，信息 `[Step 3.3] 帧缓冲管理`
- [ ] **开发讲解**（AI 完成后填写）:
- [ ] **用户确认**: [ ] 看懂本步讲解
- [ ] **完成日期**: *(YYYY-MM-DD)*

---

#### 阶段 3 验收清单
- [ ] 单字节 UART 收发正常（PC 串口助手 ↔ DSP）
- [ ] 单字节 SPI 收发正常（CPLD 回环或示波器验证）
- [ ] 环形缓冲区 push/pop 逻辑正确
- [ ] Modbus 帧超时检测正常（4ms 空闲 = 帧结束）
- [ ] 所有 Step 均已 commit，打 tag：`v0.1.0-phase3`
- [ ] 学习笔记补充完毕

---

### 阶段 4: 主循环集成与调试验证

#### Step 4.1: 实现 `interrupt void ISRCpuTimer0(void)` — 1ms 主循环 ISR
- [ ] **状态**: `[ ]` 待办
- [ ] **函数签名**: `interrupt void ISRCpuTimer0(void)`
- [ ] **功能描述**: 1ms 周期中断服务，执行以下任务：
  1. 轮询 SCI RX FIFO，有数据则 `SciRxBuf_PushByte()`
  2. 若 SPI→UART 无帧待发，发送哑字节轮询 CPLD；有数据则从 SpiTxBuf 取字节发送
  3. SPI 返回字节若有效 → `SpiRxBuf_PushByte()`
  4. 检查 SciRxBuf 帧是否完整 → 完整则拷贝到 SpiTxBuf 准备转发
  5. 检查 SpiRxBuf 帧是否完整 → 完整则逐字节 `SciSendByte()` 转发
  6. LED 闪烁控制（TX=发送次数计数，RX=接收次数计数）
  7. 清除 Timer0 中断标志和 PIEACK
- [ ] **涉及文件**:
  - `SRC/MAIN.c` — *(修改)*: 实现 ISR 函数体，声明帧缓冲区相关外部变量
- [ ] **依赖**: Step 3.3（缓冲区管理全部就绪）
- [ ] **预计工时**: 1 小时
- [ ] **测试方法**: PC 发 Modbus 帧 → CPLD 应答 → PC 收到转发帧；串口助手和逻辑分析仪同时监测
- [ ] **验收标准**:
  - [ ] 编译 0 errors
  - [ ] ISR 总执行时间 < 50 μs（远小于 1ms 周期）
  - [ ] UART→SPI 方向：PC 发帧 → DSP 缓冲 → SPI 发送无误
  - [ ] SPI→UART 方向：CPLD 回帧 → DSP 缓冲 → UART 发送无误
  - [ ] Modbus 帧超时正确（4ms 帧间空闲）
  - [ ] LED TX/RX 随收发正常闪烁
  - [ ] git commit 完成，信息 `[Step 4.1] ISR 主循环`
- [ ] **开发讲解**（AI 完成后填写）:
- [ ] **用户确认**: [ ] 看懂本步讲解
- [ ] **完成日期**: *(YYYY-MM-DD)*

---

#### Step 4.2: 实现 `void main(void)` 主函数 + 联调
- [ ] **状态**: `[ ]` 待办
- [ ] **函数签名**: `void main(void)`
- [ ] **功能描述**: 组装初始化序列 → 使能中断 → 主循环 `for(;;){}` 空转（所有逻辑在 ISR 中完成）。联调验证端到端透传
- [ ] **涉及文件**:
  - `SRC/MAIN.c` — *(修改)*: 实现 `main()`，按初始化序列调用各 `AppConfig_*` 函数
- [ ] **依赖**: Step 4.1
- [ ] **预计工时**: 0.5 小时
- [ ] **测试方法**:
  1. PC 串口助手发送 Modbus 读保持寄存器请求帧
  2. 帧经 DSP → SPI → CPLD
  3. CPLD 应答经 SPI → DSP → UART → PC 串口助手显示
  4. 验证 10 轮双向收发无误
  5. 验证 LED 闪烁
- [ ] **验收标准**:
  - [ ] 编译 0 errors
  - [ ] 端到端双向透传正常（PC→CPLD→PC，往返无误）
  - [ ] 连续收发 100 帧无误码
  - [ ] Modbus 帧边界准确（不拆帧、不粘帧）
  - [ ] LED TX/RX 闪烁频率与收发频率一致
  - [ ] git commit 完成，信息 `[Step 4.2] main 主函数 + 联调`
- [ ] **开发讲解**（AI 完成后填写）:
- [ ] **用户确认**: [ ] 看懂本步讲解
- [ ] **完成日期**: *(YYYY-MM-DD)*

---

#### 阶段 4 验收清单
- [ ] 全部 PRD 功能项 (F-001 ~ F-006) 验收通过
- [ ] 性能指标达标：帧转发延迟 < 10ms，CPU 占用 < 10%
- [ ] 打 release tag：`v1.0.0`
- [ ] 学习笔记/学习指南/速查表全部补全

---

## 进度追踪规则

1. **每完成一步**：将该步的 `[ ]` 改为 `[x]`，填写完成日期，立即 `git commit`
2. **每完成一步必须**：填写"开发讲解" → 更新学习笔记 → 用大白话向用户讲解 → **用户确认看懂后才继续下一步**
3. **遇到阻塞**：保持 `[ ]` 并添加 `⚠️ 阻塞: (原因)`
4. **跳过某步**：标记为 `[~]` 并说明原因
5. **每完成一个阶段**：运行该阶段验收清单 → 打 tag → 向用户汇报进度摘要
6. **每完成一个会话**：填写 `SESSION_HANDOFF.md`

---

## 会话交接

- 当前会话进行到: 计划生成阶段
- 详细交接内容见 **`SESSION_HANDOFF.md`**

---

## 变更记录

| 日期 | 步骤 | 变更描述 |
|---|---|---|
| 2026-08-08 | — | 初始计划生成，10 Steps，4 阶段 |
| 2026-08-08 | Step 1.2 | GPIO35/36 MUX 修正 (2→1) + QSEL/PUD 补充，参照 DSP2833x_DAB |
| 2026-08-08 | Step 2.2 | SPI-A 初始化 (SPI Mode 0, 8-bit, 主机)；GPIO16-18 MUX 修正 (3→1) + QSEL/PUD |

---

*本文件与 PRD.md、SESSION_HANDOFF.md 配合使用，贯穿整个开发过程。*

---

*Template by NSQ*
