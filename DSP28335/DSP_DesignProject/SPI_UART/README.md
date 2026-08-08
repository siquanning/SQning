# DSP 项目模板 — TMS320F28335 | NSQ

> **致 AI Agent：每次启动时，请按顺序阅读四件套文档再与用户交互：**
> ① `README.md`（本文件，工作说明书）→ ② `PRD.md` → ③ `IMPLEMENTATION_PLAN.md` → ④ `SESSION_HANDOFF.md`（会话交接记录，恢复上次进度）
> **学习文档**（`LEARNING_NOTES.md`、`docs/`）在涉及新知识点时阅读并按规则更新。

---

## 1. 工程用途

这是一个基于 **TI TMS320F28335** (C2000 Delfino) 的空白 DSP 工程模板。目标是为所有新 DSP 项目提供统一起点──复制本模板、重命名项目、填写 PRD，即可通过 AI 辅助（Vibe Coding）方式完成开发。**本模板同时是学习模板：AI 开发时优先使用 TI 标准库与封装函数，并逐步讲解，让用户跟着看懂 DSP 是怎么开发的。**

### 技术栈

| 项 | 值 |
|---|---|
| 芯片 | TMS320F28335 (32-bit C28x + FPU) |
| 编译器 | TI C2000 v6.2.7, COFF 格式 |
| 构建系统 | CCS Managed Make (GNU Make) |
| 调试器 | XDS100v3 USB JTAG |
| 数学库 | IQMath, FPUmathTables (Boot ROM) |
| 主频 | 150 MHz (30 MHz XTAL × 10 PLL / 2 DIVSEL) |

---

## 2. 标准工作流 (AI 必须严格遵守)

### 阶段 1: PRD 讨论 → 填写 PRD.md

1. 主动向用户提问，全面理解需求：功能、性能指标、硬件平台、外部接口、保护逻辑
2. 将讨论结果填入根目录的 **`PRD.md`**（按模板章节填写）
3. **PRD 未经用户确认，不得编写任何代码**

### 阶段 2: 生成分步实施计划 → 填写 IMPLEMENTATION_PLAN.md

1. 基于已确认的 `PRD.md`，生成 **`IMPLEMENTATION_PLAN.md`**
2. 计划采用**函数级微步骤**粒度（详见 §3 计划模板），每个步骤必须包含：
   - 步骤编号和名称（**标题直接写函数签名**）
   - 函数签名 / 宏名、涉及文件、依赖项、预计工时
   - 测试方法和验收标准
   - **开发讲解**字段（AI 完成后填写）和**用户确认**项
   - 完成标志（`[ ]` → `[x]` + 完成日期）
3. 步骤划分原则：**每步 = 1 个函数（或 1 个紧密函数对），可独立编译、可独立验证、不超过 3 个文件**
   - 反例：❌ "实现 ADC 模块" → 应拆成 初始化/单通道采样/多通道轮询/中断触发/均值滤波 等多个 Step
4. **同步创建学习文档**：确认 `docs/TI_LIB_QUICKREF.md` 覆盖本项目用到的库函数；创建 `docs/DSP_LEARNING_GUIDE.md` 并填充外设清单与章节框架
5. **计划未经用户确认，不得开始编码**

### 阶段 3: 逐步编码实现（函数级微步骤）

1. 严格按照 `IMPLEMENTATION_PLAN.md` 的顺序逐步执行
2. 每完成一个 Step（1 个函数）：
   - 在 `IMPLEMENTATION_PLAN.md` 中标记 `[x]` + 完成日期
   - 更新相关代码注释（必须简体中文）
   - **必须**提交一次 Git commit（信息格式 `[Step X.Y] 中文描述`）
   - **必须填写"开发讲解"**：用了哪些 TI 库函数 / 为什么这样做 / 硬件原理一句话
   - **必须更新学习笔记** `LEARNING_NOTES.md`（新知识点追加到对应主题）
   - 注释语言检查：运行 `python tools/check_comments.py`（必须通过）
3. **学习确认（每步必须）**：AI 用大白话向用户讲解本步做了什么、为什么；然后询问"这一步看懂了吗"。**用户未确认看懂 → AI 必须换更简单的说法重讲，不得继续下一步**
4. **只修改当前步骤涉及的文件**，不得触碰无关模块
5. 所有代码必须遵循本工程的编码规范（参见 §4），**应用层只准用封装函数**

### 阶段 4: 会话交接 → 填写 SESSION_HANDOFF.md（AI 接力）

> 单个 AI 窗口上下文有限，长对话或换新窗口时必须交接现场状态。

1. **每次会话结束前**：在 `SESSION_HANDOFF.md` 追加一条交接记录，内容包括：
   - 当前进度快照（对照实施计划：已完成/进行中/下一步）
   - 已完成功能清单、遗留问题与已知坑
   - 未提交的本地改动、当前 git 分支/HEAD/tag
   - 关键决策与约定、**下一个 AI 的下一步指引**
2. **新会话启动时**：先读四件套（README → PRD → 计划 → SESSION_HANDOFF），向用户复述"当前进度 + 下一步计划"，确认后继续
3. 交接记录保留历史，不覆盖；每条记录以会话 # 编号

---

## 3. 目录结构与规范文档

```
{{PROJECT_NAME}}/
├── README.md                  ← 本文件 (AI 工作说明书, 每次会话必读)
├── PRD.md                     ← 产品需求文档 (阶段1填写)
├── IMPLEMENTATION_PLAN.md     ← 分步实施计划 (阶段2生成, 阶段3跟踪; 函数级微步骤)
├── SESSION_HANDOFF.md         ← 会话交接文档 (每次会话结束填写, AI 接力用)
├── LEARNING_NOTES.md          ← 学习笔记 (每步知识点沉淀, 用户跟着学)
├── .vibe-coding-rules         ← AI 行为约束规则
├── .ccsproject                ← CCS 工程配置 (芯片型号/调试器)
├── .cproject                  ← CCS 构建配置 (编译器/链接器选项)
├── .project                   ← Eclipse 工程描述 (工程名称)
├── .clangd                    ← clangd 语言服务器配置
├── 28335_RAM_lnk.cmd          ← RAM 调试链接器脚本 (Debug 配置)
├── CMD/
│   ├── F28335.cmd             ← Flash 烧写链接器脚本 (Release 配置)
│   └── DSP2833x_Headers_nonBIOS.cmd ← 外设寄存器内存映射
├── docs/
│   ├── TI_LIB_QUICKREF.md     ← TI 库函数速查表 (封装 vs 寄存器对照, 常用函数)
│   └── DSP_LEARNING_GUIDE.md  ← 项目学习指南 (外设原理/库函数/配置/常见坑)
├── INCLUDE/
│   ├── DSP2833x_*.h           ← TI DSP2833x 外设寄存器定义头文件
│   ├── IQmathLib.h            ← IQ Math 定点数学库头文件
│   ├── SFO.h / SFO_V5.h       ← HRPWM 比例因子优化器头文件
│   └── APP_CONFIG.h           ← 【用户修改】应用层配置 (引脚/常量/接口)
├── SRC/
│   ├── MAIN.c                 ← 【用户修改】主入口 + ISR 模板
│   ├── APP_CONFIG.c           ← 【用户修改】GPIO 和外设初始化 (驱动层)
│   ├── DSP2833x_*.c/.asm      ← TI 外设库 (不要修改)
│   ├── DSP2833x_CodeStartBranch.asm ← Boot 入口
│   └── DSP2833x_GlobalVariableDefs.c ← 外设寄存器全局变量
├── tools/
│   └── check_comments.py      ← 注释语言检查脚本 (必须简体中文)
├── reference/                 ← 参考资料 (原理图/参考代码/相关项目/数据手册)
│   ├── README.md              ← 参考资料使用说明
│   ├── schematics/            ← 原理图、PCB 图、接线图
│   ├── code/                  ← 参考代码片段或示例工程
│   ├── projects/              ← 相关项目说明/接口文档
│   └── datasheets/            ← 芯片数据手册、应用笔记
├── targetConfigs/
│   └── TMS320F28335.ccxml     ← JTAG 目标配置 (XDS100v3 + F28335)
├── .launches/
│   └── SPI_UART.launch        ← 调试启动配置模板
└── .settings/                 ← Eclipse/CDT 工作空间偏好
```

### 用户需要修改的文件

| 文件 | 何时修改 | 修改内容 |
|---|---|---|
| `.project` | 创建新项目时 | `<name>` 改为实际工程名 |
| `PRD.md` | 阶段1 | 填写产品需求 |
| `IMPLEMENTATION_PLAN.md` | 阶段2-3 | 生成计划（函数级微步骤）和跟踪进度 |
| `SESSION_HANDOFF.md` | 阶段4 | 每次会话结束填写交接记录 |
| `LEARNING_NOTES.md` | 阶段3 | AI 每步追加知识点（用户可补充自己的疑问） |
| `docs/DSP_LEARNING_GUIDE.md` | 阶段2-6 | AI 建框架并逐步补全外设讲解 |
| `docs/TI_LIB_QUICKREF.md` | 阶段3 | AI 遇到新库函数时补充 |
| `reference/*` | 任意时刻 | 放入原理图、参考代码、数据手册等外部资料（不参与编译） |
| `SRC/MAIN.c` | 阶段3 | 添加 ISR 和应用逻辑 |
| `SRC/APP_CONFIG.c` | 阶段3 | 添加 GPIO 和外设初始化 |
| `INCLUDE/APP_CONFIG.h` | 阶段3 | 定义引脚宏和常量 |

### 不要修改的文件

- `SRC/DSP2833x_*.c/.asm` — TI 外设库, 保持原样
- `INCLUDE/DSP2833x_*.h` — TI 头文件, 保持原样
- `CMD/*.cmd` — 链接器脚本, 除非有特殊内存需求
- `.ccsproject`, `.cproject` — 构建配置, 除非更换芯片型号

---

## 4. 编码规范

### C 代码风格
- 函数名: `PascalCase_Init()` — 模块前缀 + 下划线 + 操作
- 变量名: `camelCase` — 局部变量以模块前缀开头
- 宏定义: `UPPER_SNAKE_CASE`
- 缩进: 4 空格，不用 Tab

### 封装函数优先（双层策略）★学习核心
> 目标：让用户看得懂。能调库函数的地方绝不写寄存器；必须写寄存器的地方逐行中文注释解释。

- **应用层**（`MAIN.c` 的主循环、算法、ISR 逻辑）：只准调用 TI 库函数与 `AppConfig_*` 封装，**禁止裸寄存器操作**
- **驱动层**（`APP_CONFIG.c`）：允许寄存器操作，但**每行必须中文注释**解释"这个寄存器在做什么"
- **例外**：TI 无封装的必要操作（如 ISR 末尾 `PieCtrlRegs.PIEACK.all = PIEACK_GROUPx;`、`EALLOW/EDIS` 包裹的向量注册）允许出现在应用层，但必须注释
- 对照示例见 `docs/TI_LIB_QUICKREF.md` §0（封装函数 vs 裸寄存器对比表）

### 注释语言（强制规则）
- **所有新增/修改的注释必须使用简体中文**，禁止英文描述性注释
- 例外（允许保留原文）：代码标识符、寄存器名、宏名（如 `PieCtrlRegs`、`EALLOW`、`CPU_CLK`、`GPIO5`）、Doxygen 标签（`@file`/`@brief` 等）、C 关键字、常见技术缩写（ADC/PWM/ISR/Trip-Zone 等）
- **每次提交前运行检查**：`python tools/check_comments.py`（必须输出 `[通过]`）
- 注释模板（照此风格书写）：

```c
/**
 * @file    模块名.c
 * @brief   一句话说明本模块用途
 *
 * 接口约定:
 *   - 函数调用时机/频率
 *   - 参数单位
 */
/**
 * @brief 函数功能一句话
 *
 * @param  参数名 参数说明（含单位）
 * @return 返回值说明
 */
/* 行内注释用中文，说明"为什么"而非"做了什么" */
```

### 中断服务例程 (ISR) 规范
- 使用 `interrupt void ISRXxx(void)` 声明
- ISR 必须尽量短小 (建议 < 100 行)
- ISR 末尾**必须**清除 PIEACK: `PieCtrlRegs.PIEACK.all = PIEACK_GROUPx;`
- CPU Timer ISR 还必须: `CpuTimer0Regs.TCR.bit.TIF=1; CpuTimer0Regs.TCR.bit.TRB=1;`
- 实时控制逻辑放 ISR 中，非实时任务放 `for(;;)` 主循环中

### GPIO 操作规范
```c
// SET/CLEAR 操作 (原子操作, 不干扰同端口其他引脚)
#define SET_LED     GpioDataRegs.GPASET.bit.GPIO5 = 1
#define CLEAR_LED   GpioDataRegs.GPACLEAR.bit.GPIO5 = 1

// 读取操作
#define READ_FAULT  GpioDataRegs.GPADAT.bit.GPIO6
```

### 安全要求
- **禁止**使用 `for(;;);` 空循环做延时 (应使用 `DELAY_US()`)
- **禁止**在 ISR 中调用阻塞函数
- 跨 ISR/主循环的共享变量必须用 `volatile` 修饰
- 所有 EALLOW 保护的寄存器写入后, 必须有 EDIS

---

## 5. 常用系统初始化序列

```c
void main(void) {
    InitSysCtrl();              // 1. PLL=150MHz, 关看门狗, 使能外设时钟
    // InitXintf16Gpio();       //    可选: XINTF GPIO
    // InitXintf();             //    可选: XINTF 时序
    DINT;                       // 2. 关中断
    InitPieCtrl();              // 3. 初始化 PIE
    IER=0; IFR=0;
    InitPieVectTable();         // 4. 加载默认中断向量表
    // 注册用户 ISR ...         // 5. EALLOW; PieVectTable.xxx = &ISR;
    InitCpuTimers();            // 6. 初始化定时器
    // ConfigCpuTimer(...);     //    配置定时器周期
    // 外设初始化...            // 7. ePWM, ADC, SCI, SPI, ...
    // 使能中断...              // 8. IER, PIEIER, ENPIE
    EINT; ERTM;                 // 9. 开全局中断
    // 启动定时器/PWM...        // 10. 开始实时控制
    for(;;) { /* 非实时任务 */ }
}
```

---

## 6. 构建命令

在 CCS IDE 中：
- **构建**: Project → Build Project (Ctrl+B)
- **调试**: Run → Debug (F11) — 使用 `.launches/SPI_UART.launch`
- **切换配置**: Project → Build Configurations → Set Active (Debug / Release)

在 CCS 工作区导入工程：
1. Project → Import CCS Projects...
2. 选择 `{{PROJECT_NAME}}` 目录
3. 确保 "Copy projects into workspace" 未勾选

---

## 7. 版本管理与回滚手册（防"改坏已实现代码"）

> 原则：**每步必提交，每阶段必打 tag，调试前必打快照**。任何时刻都能回到任意已知良好版本。

### 7.1 日常提交节奏（AI 必须遵守）

| 时机 | 操作 | 示例 |
|---|---|---|
| 首次使用模板 | `git init`（创建独立版本库） | `git init` |
| 每完成一个 Step | 提交一次 | `git add -A && git commit -m "[Step 2.3] 实现 AppConfig_ReadADC"` |
| 每完成一个阶段 | 打阶段 tag | `git tag v0.1.0-phase2` |
| 调试/实验前 | 打快照 tag（或 stash） | `git tag debug-snap-20260803` |
| 发布 | 打 release tag | `git tag v1.0.0` |

- 提交信息格式：`[Step X.Y] 中文描述`
- 一个 Step 一个 commit，不要攒多个 Step 一起提交

### 7.2 回滚手册（调试改坏代码时）

| 场景 | 命令 |
|---|---|
| 只回退单个文件到上次提交 | `git checkout -- SRC/MAIN.c` |
| 回退单个文件到某 tag/commit | `git checkout v0.1.0-phase2 -- SRC/APP_CONFIG.c` |
| 查看提交历史 | `git log --oneline --graph` |
| 查看所有 tag | `git tag -l` |
| 丢弃所有未提交改动（危险，先确认） | `git checkout -- .` |
| 回到某 tag 的整体状态 | `git checkout <tag>` |
| 临时保存当前调试改动、恢复干净状态 | `git stash`（之后 `git stash pop` 恢复） |

### 7.3 调试黄金法则
1. **动手改代码前**，先 `git status` 确认工作区干净，或先打快照 tag
2. 一次只改一个点，改完立刻验证
3. 改坏了 → 对照 §7.2 一键回退，**不要手忙脚乱继续改**
4. 确认改对了 → 提交新 commit（保留历史，不覆盖旧版本）

---

## 8. 内存布局 (关键地址)

| 区域 | 地址范围 | 大小 | 用途 |
|---|---|---|---|
| M0 SARAM | 0x000050–0x000400 | 1K | 数据/小代码 |
| M1 SARAM | 0x000400–0x000800 | 1K | 栈 (0x300) |
| L0-L3 SARAM | 0x008000–0x00C000 | 16K | 程序/数据 |
| L4-L7 SARAM | 0x00C000–0x010000 | 16K | 数据 |
| Flash A-H | 0x300000–0x33FFF8 | 256K | 程序 (Release) |
| IQTABLES | 0x3FE000 | 2.8K | IQ Math (Boot ROM) |
| FPUTABLES | 0x3FEBDC | 1.7K | FPU Math (Boot ROM) |
| ZONE7 (XINTF) | 0x200000–0x210000 | 64K | 外部总线 (ADC/CPLD) |

---

## 9. 常见外设快速参考

| 外设 | GPIO 复用函数 | 头文件 | 中断向量 |
|---|---|---|---|
| ePWM1 | `InitEPwm1Gpio()` | DSP2833x_EPwm.h | EPWM1_INT |
| ePWM2 | `InitEPwm2Gpio()` | DSP2833x_EPwm.h | EPWM2_INT |
| ADC | 芯片内置 | DSP2833x_Adc.h | ADCINT |
| SCI-A | `InitSciaGpio()` | DSP2833x_Sci.h | SCIRXINTA / SCITXINTA |
| SCI-B | `InitScibGpio()` | DSP2833x_Sci.h | SCIRXINTB / SCITXINTB |
| SPI-A | `InitSpiaGpio()` | DSP2833x_Spi.h | SPIRXINTA / SPITXINTA |
| eCAN-A | `InitECanaGpio()` | DSP2833x_ECan.h | ECAN0INTA / ECAN1INTA |
| CPU Timer0 | 无 GPIO | DSP2833x_CpuTimers.h | TINT0 (PIE Group1, Ch7) |
| XINTF | `InitXintf16Gpio()` | DSP2833x_Xintf.h | — |

---

*最后更新: 2026-08-03*