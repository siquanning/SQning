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
- [ ] **状态**: `[ ]` 待办
- [ ] **函数签名**: `void AppConfig_InitGpio(void)`
- [ ] **功能描述**: 手动配置 SPI-A 引脚（GPIO16-18 MUX=3, GPIO19 MUX=0 输出低, 方案A单从机）、SCI-A（手动配 GPIO35/36 MUX=2）、LED（GPIO67/68 输出）
- [ ] **涉及文件**:
  - `SRC/APP_CONFIG.c` — *(修改)*: 填写 `AppConfig_InitGpio()` 函数体
  - `INCLUDE/APP_CONFIG.h` — *(修改)*: 定义 LED 引脚宏（`SET_LED_TX`、`CLEAR_LED_TX`、`SET_LED_RX`、`CLEAR_LED_RX`）
- [ ] **依赖**: Step 1.1
- [ ] **预计工时**: 0.5 小时
- [ ] **测试方法**: 单步调试，观察 GPxMUX/GPxDIR 寄存器值；LED 测试点亮/熄灭
- [ ] **验收标准**:
  - [ ] 编译 0 errors
  - [ ] 注释全部简体中文
  - [ ] SPI 引脚：GPIO16(SIMO)/17(SOMI)/18(CLK) MUX=3；GPIO19 MUX=0、DIR=输出、拉低 (方案A)
  - [ ] SCI 引脚（GPIO35/36）MUX=2（SCI-A 备选位置）
  - [ ] LED 引脚（GPIO67/68）MUX=0、DIR=输出
  - [ ] git commit 完成，信息 `[Step 1.2] AppConfig_InitGpio`
- [ ] **开发讲解**（AI 完成后填写）:
- [ ] **用户确认**: [ ] 看懂本步讲解
- [ ] **完成日期**: *(YYYY-MM-DD)*

---

#### 阶段 1 验收清单
- [ ] 工程编译通过
- [ ] GPIO 初始化代码就绪，调试器中可验证 MUX/DIR 寄存器
- [ ] 所有 Step 均已 commit，打 tag：`v0.1.0-phase1`
- [ ] 学习笔记已补充阶段 1 知识点

---

### 阶段 2: 外设驱动初始化

#### Step 2.1: 实现 `void AppConfig_InitSci(void)` — 初始化 SCI-A UART
- [ ] **状态**: `[ ]` 待办
- [ ] **函数签名**: `void AppConfig_InitSci(void)`
- [ ] **功能描述**: 配置 SCI-A：9600 bps / 8N1 / 无硬件流控 / 使能 TX RX / FIFO 8 字节
- [ ] **涉及文件**:
  - `SRC/APP_CONFIG.c` — *(修改)*: 填写 `AppConfig_InitSci()` 函数体
  - `INCLUDE/APP_CONFIG.h` — *(修改)*: 定义 `SCI_BAUD_9600` 等宏
- [ ] **依赖**: Step 1.2（GPIO 必须先配好 MUX）
- [ ] **预计工时**: 0.5 小时
- [ ] **测试方法**: 调试器中检查 SCI-A 寄存器（SCIHBAUD/SCILBAUD/SCICCR/SCICTL1/SCICTL2）；用串口助手发字符，在 FIFO 寄存器中验证接收
- [ ] **验收标准**:
  - [ ] 编译 0 errors
  - [ ] 注释全部简体中文
  - [ ] 波特率寄存器值对应 9600 bps @150MHz LSPCLK
  - [ ] SCICCR = 8-bit, 1 stop, no parity
  - [ ] FIFO 使能（SCIFFTX/SCIFFRX），RX FIFO 深度 8
  - [ ] git commit 完成，信息 `[Step 2.1] AppConfig_InitSci`
- [ ] **开发讲解**（AI 完成后填写）:
- [ ] **用户确认**: [ ] 看懂本步讲解
- [ ] **完成日期**: *(YYYY-MM-DD)*

---

#### Step 2.2: 实现 `void AppConfig_InitSpi(void)` — 初始化 SPI-A 主机
- [ ] **状态**: `[ ]` 待办
- [ ] **函数签名**: `void AppConfig_InitSpi(void)`
- [ ] **功能描述**: 配置 SPI-A 为主机模式、8-bit 数据、波特率匹配 9600 bps、使能发送接收
- [ ] **涉及文件**:
  - `SRC/APP_CONFIG.c` — *(修改)*: 填写 `AppConfig_InitSpi()` 函数体
  - `INCLUDE/APP_CONFIG.h` — *(修改)*: 定义 SPI 波特率宏
- [ ] **依赖**: Step 1.2（GPIO 必须先配好 MUX）
- [ ] **预计工时**: 0.5 小时
- [ ] **测试方法**: 调试器中检查 SPI-A 寄存器（SPICCR/SPICTL/SPIBRR）；用逻辑分析仪/示波器观察 CLK 引脚波形
- [ ] **验收标准**:
  - [ ] 编译 0 errors
  - [ ] 注释全部简体中文
  - [ ] SPIBRR 对应 SPI CLK ≈ 9600 bps（或接近值）
  - [ ] 主机模式，8-bit，无相位滞后，下降沿输出
  - [ ] CS（GPIO19/SPISTE）由 SPI 模块自动管理
  - [ ] git commit 完成，信息 `[Step 2.2] AppConfig_InitSpi`
- [ ] **开发讲解**（AI 完成后填写）:
- [ ] **用户确认**: [ ] 看懂本步讲解
- [ ] **完成日期**: *(YYYY-MM-DD)*

---

#### Step 2.3: 实现 `void AppConfig_InitCpuTimer0(void)` — 初始化 1ms 时基
- [ ] **状态**: `[ ]` 待办
- [ ] **函数签名**: `void AppConfig_InitCpuTimer0(void)`
- [ ] **功能描述**: 配置 CPU Timer0 以 1ms 周期触发中断，作为主循环时基和 Modbus 帧超时计时
- [ ] **涉及文件**:
  - `SRC/APP_CONFIG.c` — *(修改)*: 填写 `AppConfig_InitCpuTimer0()` 函数体
  - `INCLUDE/APP_CONFIG.h` — *(修改)*: 定义 `TIMER0_PERIOD_MS` 宏
  - `SRC/MAIN.c` — *(修改)*: 注册 Timer0 ISR 向量（EALLOW + PieVectTable）
- [ ] **依赖**: Step 2.1, 2.2（外设初始化可并行，无依赖）
- [ ] **预计工时**: 0.5 小时
- [ ] **测试方法**: 在 ISR 中翻转 LED，用示波器测量翻转周期 = 2ms（频率 500Hz）；或用另一个定时器测量
- [ ] **验收标准**:
  - [ ] 编译 0 errors
  - [ ] 注释全部简体中文
  - [ ] ConfigCpuTimer() 参数对应 1ms 周期 @150MHz SYSCLKOUT
  - [ ] ISR 向量正确注册，PIE 使能
  - [ ] 示波器实测翻转周期 = 2ms ±5%
  - [ ] git commit 完成，信息 `[Step 2.3] AppConfig_InitCpuTimer0`
- [ ] **开发讲解**（AI 完成后填写）:
- [ ] **用户确认**: [ ] 看懂本步讲解
- [ ] **完成日期**: *(YYYY-MM-DD)*

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
- [ ] **状态**: `[ ]` 待办
- [ ] **函数签名**:
  - `Uint16 SciReceiveByte(void)` — 从 SCI RX FIFO 读 1 字节，无数据返回 0xFFFF
  - `void SciSendByte(Uint16 byte)` — 将 1 字节写入 SCI TX FIFO
- [ ] **功能描述**: 封装 SCI FIFO 的读写操作，供主循环调用
- [ ] **涉及文件**:
  - `SRC/APP_CONFIG.c` — *(修改)*: 添加两个函数
  - `INCLUDE/APP_CONFIG.h` — *(修改)*: 声明函数原型
- [ ] **依赖**: Step 2.1（SCI 必须已初始化）
- [ ] **预计工时**: 0.5 小时
- [ ] **测试方法**: 调试器设断点，串口助手发单字节，验证 SciReceiveByte() 返回值
- [ ] **验收标准**:
  - [ ] 编译 0 errors
  - [ ] 应用层调用不直接操作寄存器（通过封装函数）
  - [ ] SciReceiveByte() 空 FIFO 时返回 sentinel 值，不会阻塞
  - [ ] SciSendByte() 发送后可在 PC 串口助手中看到对应字符
  - [ ] git commit 完成，信息 `[Step 3.1] UART 字节收发`
- [ ] **开发讲解**（AI 完成后填写）:
- [ ] **用户确认**: [ ] 看懂本步讲解
- [ ] **完成日期**: *(YYYY-MM-DD)*

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

---

*本文件与 PRD.md、SESSION_HANDOFF.md 配合使用，贯穿整个开发过程。*

---

*Template by NSQ*
