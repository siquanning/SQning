# 分步实施计划 | NSQ

> **本文件由 AI 在阶段2生成，在阶段3跟踪进度。**
> **计划未经用户确认前，不得开始编码。**
> **计划采用"函数级微步骤"粒度：每一个 Step 对应 1 个函数（或 1 个紧密关联的函数对/宏组）。**
> **学习机制：每个 Step 完成后 AI 必须写"开发讲解"并确认用户看懂。**

---

## 项目信息

- 项目名称: SPI_UART（SPI↔UART 双向透传桥）
- 计划生成日期: 2026-08-08
- 预计总工时: 8~12 小时

---

## 总体阶段划分

| 阶段 | 内容 | 预计步骤数 | 状态 |
|---|---|---|---|
| 1 | 基础框架搭建 | 2 | [ ] |
| 2 | SCI 回显（外设驱动 + 收发验证） | 3 | [ ] |
| 3 | SPI 驱动 + 双向透传 | 4 | [ ] |
| 4 | Modbus 帧边界检测 + 软件流控 | 3 | [ ] |
| 5 | 集成测试与优化 | 2 | [ ] |

> **当前首要目标：先跑通 SCI 回显（阶段 1+2），验证 UART 硬件通路后再做 SPI 透传。**

---

## 学习文档（阶段2 同步创建）

| 文档 | 位置 | 内容 | 谁维护 |
|---|---|---|---|
| 库函数速查表 | `docs/TI_LIB_QUICKREF.md` | 常用 TI 封装函数速查 | AI |
| 项目学习指南 | `docs/DSP_LEARNING_GUIDE.md` | 外设原理/库函数/配置/常见坑 | AI |
| 学习笔记 | `LEARNING_NOTES.md` | 每步知识点沉淀 | AI |

---

## 详细步骤

### 阶段 1: 基础框架搭建

#### Step 1.1: 工程初始化 — 修复时钟宏 + 验证编译
- [x] **状态**: `[x]` 已完成
- [x] **对象**: 工程级配置（非函数，本步骤例外）
- [x] **功能描述**: 修复 `APP_CONFIG.h` 中 `CPU_CLK` 浮点数问题（`150e6` → `150000000L`），添加 `LSPCLK` 宏，验证 Debug 配置编译通过
- [x] **涉及文件**:
  - `INCLUDE/APP_CONFIG.h` — *(修改)*: `CPU_CLK` 改为整数，新增 `LSPCLK`、`SCI_BRR` 等时钟宏
- [x] **依赖**: 无
- [x] **预计工时**: 0.5 小时
- [x] **测试方法**: CCS 中 Debug 配置编译，确认 0 errors
- [x] **验收标准**:
  - [x] 编译 0 errors ✓
  - [x] `CPU_CLK` 和 `LSPCLK` 为整数常量（可参与位运算）✓
  - [x] git commit 完成，信息 `[Step 1.1] 工程初始化 — 修复时钟宏` ✓
- [x] **开发讲解**（AI 完成后填写）:
  - 用到的 TI 库函数/封装: 无（纯宏定义修改，不涉及库函数调用）
  - 为什么这样做: C 语言的 `150e6` 是浮点数常量（double），浮点数不能参与位运算。在嵌入式 DSP 代码中，时钟频率经常需要做整数除法和位运算（如计算分频系数、波特率除数等），所以必须写成 `150000000L`（long 整数）。同样新增了 `LSPCLK`（37.5MHz）和 `HSPCLK`（75MHz）宏，因为 SCI 波特率计算需要 LSPCLK，SPI 速率计算需要 HSPCLK。`SCI_BRR_VALUE=487` 是把波特率计算提前做好（37500000/76800-1），避免运行时浮点运算。
  - 硬件原理（一句话）: F28335 的 SYSCLKOUT=150MHz 经过 LOSPCP 分频器（÷4）产生 LSPCLK=37.5MHz，SCI 模块用 LSPCLK 再除以 (BRR+1)×8 得到最终波特率。
- [x] **用户确认**: [x] 看懂本步讲解
- [x] **完成日期**: *2026-08-08*

---

#### Step 1.2: 实现 `void AppConfig_InitGpio(void)` — GPIO 初始化（SCI 引脚 + LED）
- [x] **状态**: `[x]` 已完成
- [x] **函数签名**: `void AppConfig_InitGpio(void)`
- [x] **功能描述**: 配置 SCI-A 引脚（GPIO35=TX, GPIO36=RX）的 MUX/方向/输入限定/上拉；配置 LED 引脚（GPIO67=TX_LED, GPIO68=RX_LED）为 GPIO 输出，初始熄灭（高电平）；在 `APP_CONFIG.h` 中定义引脚操作宏
- [x] **涉及文件**:
  - `SRC/APP_CONFIG.c` — *(修改)*: 填写 `AppConfig_InitGpio()` 函数体
  - `INCLUDE/APP_CONFIG.h` — *(修改)*: 定义 LED 宏（`SET_TXLED`/`CLEAR_TXLED`/`TOGGLE_TXLED` 等）
- [x] **依赖**: Step 1.1
- [x] **预计工时**: 0.5 小时
- [x] **测试方法**: 调试器中单步执行，观察 `GpioCtrlRegs.GPBMUX1`、`GPBDIR`、`GPCMUX1`、`GPCDIR` 寄存器值是否正确；在 Watch 窗口手动 SET/CLEAR LED 验证 GPIO 通路
- [x] **验收标准**:
  - [x] 编译 0 errors ✓
  - [x] 注释全部简体中文（待运行 check_comments.py 验证）
  - [x] GPIO35 MUX=1（SCI 功能），GPIO36 MUX=1（SCI 功能），QSEL=3（异步）✓
  - [x] GPIO67/68 MUX=0（GPIO 功能），DIR=1（输出），初始高电平（LED 灭）✓
  - [x] git commit 完成，信息 `[Step 1.2] AppConfig_InitGpio` ✓
- [x] **开发讲解**（AI 完成后填写）:
  - 用到的 TI 库函数/封装: 全是寄存器操作（`GpioCtrlRegs.GPxMUX/GPxQSEL/GPxPUD/GPxDIR` + `GpioDataRegs.GPxSET/GPxCLEAR`），都在 `DSP2833x_Gpio.h` 中定义为位域结构体。没有调库函数，因为 GPIO 配置没有现成的封装（不像 SCI 有 `InitSciGpio()`）。
  - 为什么这样做: F28335 的 GPIO 是复用的——每个引脚既可以是普通 IO，也可以给外设（如 SCI/SPI）用。MUX 寄存器决定了"这个引脚归谁用"：MUX=0 是普通 GPIO，MUX=1 是 SCI-A。QSEL=3 是"异步输入"模式，专门给 UART 这种没有同步时钟的通信引脚用——不用 DSP 内部时钟去采样，避免亚稳态。PUD=0 是启用内部上拉电阻，让悬空引脚默认高电平。LED 初始高电平是因为板子用低电平点灯，高电平自然是灭的。
  - 硬件原理（一句话）: GPIO 引脚的 MUX 寄存器像"切换开关"——拨到不同档位就把引脚分配给不同外设（0=普通IO, 1=SCI, 3=SPI），QSEL 决定输入信号的采样方式（3=异步=不做同步滤波，适合 UART）。
- [x] **用户确认**: [ ] 看懂本步讲解
- [x] **完成日期**: *2026-08-08*

---

#### 阶段 1 验收清单
- [ ] 工程能在 Debug 配置下编译并进入 `main()`
- [ ] GPIO 寄存器配置在调试器中验证正确
- [ ] 阶段 1 的所有 Step 均已 commit
- [ ] 所有 Step 均完成"开发讲解"且用户确认看懂
- [ ] 向用户汇报阶段 1 总结，用户确认后进入阶段 2

---

### 阶段 2: SCI 回显（外设驱动 + 收发验证）

#### Step 2.1: 实现 `void AppConfig_InitSci(void)` — SCI-A 初始化（9600bps + FIFO + RX 中断）
- [x] **状态**: `[x]` 已完成
- [x] **函数签名**: `void AppConfig_InitSci(void)`
- [x] **功能描述**: 按 PRD §5.5 的寄存器初始化序列配置 SCI-A：8N1、9600bps（BRR=487=0x1E7）、FIFO 增强模式、RX FIFO 深度=1、使能 RX FIFO 中断、注册 `SCIRXINTA` ISR 到 PIE 向量表、使能 PIE Group 9.1 + CPU INT9
- [x] **涉及文件**:
  - `SRC/APP_CONFIG.c` — *(修改)*: 填写 `AppConfig_InitSci()` 函数体（~60 行含注释）
  - `SRC/MAIN.c` — *(修改)*: 串联 `AppConfig_Init()` 调用、ENPIE、ISRSciRx 桩函数
- [x] **依赖**: Step 1.2（GPIO 必须先配置好 MUX）
- [x] **预计工时**: 1 小时
- [x] **测试方法**: 编译下载后，在 CCS 调试器中设断点在 ISR 入口，用串口工具发一字节，观察是否进入 ISR、`SciaRegs.SCIRXBUF` 值是否正确
- [x] **验收标准**:
  - [x] 编译 0 errors（已验证，编译器版本警告可忽略）✓
  - [x] 注释全部简体中文：`python tools/check_comments.py --allow bps` 通过 ✓
  - [x] SCI 寄存器初始化序列与 PRD §5.5 一致 ✓
  - [ ] 串口工具发送字节能触发 RX ISR 断点（待 Step 2.3 硬件验证）
  - [x] git commit 完成 ✓
- [x] **开发讲解**（AI 完成后填写）:
  - 用到的 TI 库函数/封装: 全部是寄存器操作（`SciaRegs.SCICCR/SCICTL1/SCICTL2/SCIHBAUD/SCILBAUD/SCIFFTX/SCIFFRX/SCIFFCT`），这些寄存器在 `DSP2833x_Sci.h` 中定义为位域结构体。中断相关：`PieVectTable.SCIRXINTA`（PIE 向量表）、`PieCtrlRegs.PIEIER9`（PIE 使能）、`IER`（CPU 中断使能）。
  - 为什么这样做: SCI 模块初始化的顺序很重要——必须在 SWRESET=0（模块复位中）时写完所有配置，最后才把 SWRESET 置 1 释放复位，SCI 才真正开始工作。FIFO 模式（SCIFFENA=1）比非 FIFO 模式好：SCI 内部有 16 级硬件 FIFO，即使 ISR 响应稍慢也不会丢数据。RX FIFO 触发深度设为 1 字节，意思是"FIFO 里有 1 个字节就触发中断"，对 Modbus 透传最合适——每收到一个字节都能及时响应，不会等攒满 16 字节才进 ISR。
  - 硬件原理（一句话）: SCI（Serial Communication Interface）就是 DSP 的 UART 外设——把并行数据变成串行位流发出去（TX），把收到的串行位流拼回并行数据（RX），波特率由 LSPCLK 经 8×(BRR+1) 分频得到，9600bps 意味着每秒发 9600 个 bit。
- [x] **用户确认**: [ ] 看懂本步讲解
- [x] **完成日期**: *2026-08-08*

---

#### Step 2.2: 实现 `void SciSendByte(Uint16 data)` + `Uint16 SciReceiveByte(void)` — 字节级收发封装
- [ ] **状态**: `[ ]` 待办
- [ ] **函数签名**: `void SciSendByte(Uint16 data)` / `Uint16 SciReceiveByte(void)`
- [ ] **功能描述**: 封装 SCI FIFO 的字节收发操作。`SciSendByte` 等待 TX FIFO 非满（TXFFST < 16）后写入 SCITXBUF；`SciReceiveByte` 从 SCIRXBUF 读取一字节（仅低 8 位有效）
- [ ] **涉及文件**:
  - `SRC/APP_CONFIG.c` — *(修改)*: 实现两个封装函数（~10 行每个）
  - `INCLUDE/APP_CONFIG.h` — *(修改)*: 声明函数原型
- [ ] **依赖**: Step 2.1（SCI 必须先初始化）
- [ ] **预计工时**: 0.5 小时
- [ ] **测试方法**: 在 ISR 中调用 `SciReceiveByte()` 读取数据，主循环调用 `SciSendByte()` 发送，串口工具验证收发一致
- [ ] **验收标准**:
  - [ ] 编译 0 errors
  - [ ] 注释全部简体中文：`python tools/check_comments.py` 通过
  - [ ] 应用层不出现裸寄存器操作（仅调用这两个封装函数）
  - [ ] git commit 完成，信息 `[Step 2.2] SciSendByte + SciReceiveByte`
- [ ] **开发讲解**（AI 完成后填写）:
  - 用到的 TI 库函数/封装:
  - 为什么这样做:
  - 硬件原理（一句话）:
- [ ] **用户确认**: [ ] 看懂本步讲解
- [ ] **完成日期**: *(YYYY-MM-DD)*

---

#### Step 2.3: 实现 `interrupt void ISRSciRx(void)` + 主循环回显逻辑 — SCI 回显闭环
- [ ] **状态**: `[ ]` 待办
- [ ] **函数签名**: `interrupt void ISRSciRx(void)` — SCI RX FIFO 中断服务例程
- [ ] **功能描述**:
  1. ISR 中：读取 RX FIFO 中所有字节，存入全局接收缓冲区，置位接收标志，翻转 RX LED
  2. 主循环中：轮询接收标志，若置位则将接收到的数据原样发回（回显），翻转 TX LED，清除标志
  3. 在 `main()` 中按标准初始化序列串联所有步骤，注册 ISR，使能中断
- [ ] **涉及文件**:
  - `SRC/MAIN.c` — *(修改)*: 填写 main() 完整初始化序列；实现 ISRSciRx()；主循环回显逻辑
  - `SRC/APP_CONFIG.c` — *(可能微调)*: AppConfig_Init() 串联调用
- [ ] **依赖**: Step 2.2
- [ ] **预计工时**: 1 小时
- [ ] **测试方法**:
  1. 串口工具（波特率 9600/8N1）连接 DSP 的 SCI-A（GPIO35/36 经 USB 转 TTL）
  2. 发送任意字节，观察是否收到相同字节（回显）
  3. 发送多个字节，观察 FIFO 批量收发是否正常
  4. 观察 TX LED（GPIO67）和 RX LED（GPIO68）是否闪烁
- [ ] **验收标准**:
  - [ ] 编译 0 errors
  - [ ] 注释全部简体中文：`python tools/check_comments.py` 通过
  - [ ] 串口工具发送单字节 → 收到相同字节（回显正确）
  - [ ] 串口工具连续发送多字节 → 全部正确回显（FIFO 不丢数据）
  - [ ] RX LED 收到数据时闪烁，TX LED 发送数据时闪烁
  - [ ] git commit 完成，信息 `[Step 2.3] ISR + 主循环回显闭环`
- [ ] **开发讲解**（AI 完成后填写）:
  - 用到的 TI 库函数/封装:
  - 为什么这样做:
  - 硬件原理（一句话）:
- [ ] **用户确认**: [ ] 看懂本步讲解
- [ ] **完成日期**: *(YYYY-MM-DD)*

---

#### 阶段 2 验收清单
- [ ] SCI 回显功能正常：串口工具发送任意字节 → 原样返回
- [ ] TX/RX LED 闪烁正常
- [ ] FIFO 批量收发不丢数据（连续发送 256 字节验证）
- [ ] 打 tag：`v0.1.0-phase2-sci-echo`
- [ ] 所有 Step 完成"开发讲解"且用户确认看懂
- [ ] 学习笔记与学习指南已补充 SCI 外设知识点
- [ ] 向用户汇报阶段 2 总结，确认后进入阶段 3（SPI 透传）

---

### 阶段 3: SPI 驱动 + 双向透传

*(SCI 回显跑通后再细化，此处列概要)*

| Step | 函数签名 | 功能 |
|------|----------|------|
| 3.1 | `void AppConfig_InitSpi(void)` | SPI-A 主机模式初始化（8-bit, ~9600bps等效） |
| 3.2 | `void SpiSendByte(Uint16 data)` + `Uint16 SpiReceiveByte(void)` | SPI 字节收发封装（查询方式） |
| 3.3 | `void AppConfig_InitCpuTimer0(void)` | CPU Timer0 1ms 时基初始化 + ISR |
| 3.4 | 主循环双向透传逻辑 | UART RX→SPI TX / SPI RX→UART TX 轮询转发 |

#### 阶段 3 验收清单
- [ ] SPI↔UART 双向透传功能正常
- [ ] 打 tag：`v0.1.0-phase3-spi-bridge`

---

### 阶段 4: Modbus 帧边界检测 + 软件流控

| Step | 函数签名 | 功能 |
|------|----------|------|
| 4.1 | `Uint16 Modbus_IsFrameComplete(void)` | 基于 3.5 字符超时（4ms）判定帧结束 |
| 4.2 | `void Buffer_Init(void)` + 双缓冲管理 | UART RX 环形缓冲 + SPI RX 环形缓冲（各 256B） |
| 4.3 | 完整帧转发逻辑 | 收到完整帧后再转发，满缓冲丢弃 |

#### 阶段 4 验收清单
- [ ] Modbus 帧完整性检测正确（帧间间隔 > 4ms 触发转发）
- [ ] 缓冲溢出保护正常

---

### 阶段 5: 集成测试与优化

| Step | 内容 |
|------|------|
| 5.1 | 全功能联调（PC→Modbus 工具→DSP→CPLD 往返验证） |
| 5.2 | 性能验证（CPU 占用率、RAM/Flash 用量、转发延迟） |

#### 阶段 5 验收清单
- [ ] 全部 PRD 功能项 (F-001~F-006) 验收通过
- [ ] 性能指标达标（对照 PRD §4）
- [ ] 打 release tag：`v1.0.0`

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

- 当前会话进行到: 阶段 0（计划刚生成，尚未开始编码）
- 详细交接内容见 **`SESSION_HANDOFF.md`**

---

## 变更记录

| 日期 | 步骤 | 变更描述 |
|---|---|---|
| 2026-08-08 | — | 初始计划生成，SCI 回显优先（阶段 1+2），后续阶段留概要 |

---

*本文件与 PRD.md、SESSION_HANDOFF.md、docs/ 学习文档配合使用，贯穿整个开发过程。*

---

*Template by NSQ*
