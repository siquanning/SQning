# F28335_RTControl_Platform 产品需求文档（PRD）

## 1. 文档信息

| 项目 | 内容 |
|---|---|
| 产品名称 | F28335 Real-Time Control Platform |
| 工程名称 | `F28335_RTControl_Platform` |
| 当前来源 | `DROOP_SPI_UART_REFACTOR` |
| 目标芯片 | TMS320F28335，150 MHz |
| 软件形态 | 裸机、事件驱动、多速率实时控制固件平台 |
| 文档状态 | Draft v1.1 |
| 日期 | 2026-08-10 |

## 2. 名称决策

### 2.1 目标名称

统一采用：

```text
F28335_RTControl_Platform
```

对外显示名称：

```text
F28335 Real-Time Control Platform
```

### 2.2 命名理由

- `F28335`：明确芯片和工具链边界，避免误认为可直接跨MCU复用。
- `RTControl`：体现ADC采样、PWM更新、快速保护和控制算法的实时控制定位。
- `Platform`：说明当前SCI→SPI功能只是平台上的参考应用，不再定义整个工程身份。
- 不使用 `DROOP`：平台不仅服务于下垂控制，也可承载PID、PR、滤波、状态机和其他控制策略。
- 不使用 `REFACTOR`：重构是阶段性动作，不应成为长期产品名称。

### 2.3 名称迁移范围

正式改名时必须同步更新：

- 工程目录名；
- CCS `.project` 中的项目名；
- RAM Debug与Flash Release产物名；
- PowerShell与批处理构建脚本中的路径和文件名；
- 文档标题、命令示例和路径；
- 硬件测试记录中的DUT名称；
- Git软件基线标签说明。

改名不得改变寄存器配置、运行逻辑、内存布局和硬件时序。

## 3. 产品定位

`F28335_RTControl_Platform` 是面向TMS320F28335的可复用实时控制固件底座，用于承载：

- 同步ADC采样；
- ePWM产生、更新、死区与Trip Zone保护；
- 快速控制环和多速率慢速控制；
- SCI、SPI及后续CAN/I2C等通信服务；
- PID、滤波、坐标变换、下垂控制等纯算法；
- 系统状态、故障管理、参数管理和遥测；
- RAM Debug、Flash Release、Host测试和硬件回归。

当前UART→SPI链路保留为平台参考应用和回归负载，用于验证中断、队列、调度、通信和诊断基础设施。

### 3.1 双产品档位

平台提供两个编译期产品档位，共享同一套源码：

```text
PLATFORM_PROFILE_PROTOTYPE   样机档：快速开发、快速观测、尽快跑通
PLATFORM_PROFILE_INDUSTRIAL  工业档：失效安全、严格校验、可追溯发布
```

产品档位与代码装载方式彼此独立：

```text
产品档位：Prototype / Industrial
构建方式：RAM Debug / Flash Release
```

允许组合：

| 产品档位 | RAM Debug | Flash Release | 典型用途 |
|---|---:|---:|---|
| Prototype | 支持 | 支持 | 首次点灯、低压样机、算法和通信联调 |
| Industrial | 支持 | 支持 | 工业逻辑调试、生产候选版本和正式发布 |

推荐的标准产物为：

```text
Prototype_RAM_Debug
Prototype_Flash_Demo
Industrial_RAM_Debug
Industrial_Flash_Release
```

不得维护两套复制出来的源码。档位差异必须通过集中配置、编译期开关和可测试策略实现。

### 3.2 不可关闭的最低安全线

即使是Prototype档，也不得关闭：

- PWM上电默认无有效功率输出；
- 占空比、相位和比较值硬限幅；
- ePWM Trip Zone紧急关断能力；
- PLL、关键时钟和关键外设初始化失败时禁止进入RUN；
- 通信参数不能绕过范围检查直接写入快速控制状态；
- ISR中禁止动态内存、格式化输出、阻塞等待和无界循环；
- 故障和队列溢出必须可诊断。

Prototype的“快速”来自减少非关键自检、采用开发默认参数和增加调试可见性，不来自移除功率级最低保护。

### 3.3 四步总览

平台演进严格按以下四步推进；每一步完成验收后，才进入下一步。

| 步骤 | 阶段 | 核心产出 | 进入下一步的条件 |
|---|---|---|---|
| 第一步 | 平台身份与工程洁净化 | 新名称、双档位构建、工具链统一、质量门 | 改名后RAM/Flash构建、Host测试和原有通信回归全部通过 |
| 第二步 | ADC/ePWM/Trip Zone硬件底座 | 同步采样、PWM影子更新、默认安全和硬件关断 | ADC/PWM时序、Trip Zone和快速ISR WCET实测通过 |
| 第三步 | 控制、状态、参数与遥测平台 | Control/Algorithm接口、FAULT状态、参数原子提交、遥测快照 | 控制Host测试、故障路径、参数切换和实时性验证通过 |
| 第四步 | 生产验证与可复用发布 | Flash独立启动、波形、故障注入、长稳和正式发布 | 最终硬件测试矩阵、WCET、长稳和发布证据完成 |

```text
第一步：把“重构工程”改造成有正式身份的平台
    ↓
第二步：打通安全的 ADC → 控制ISR → PWM 硬件闭环
    ↓
第三步：把控制算法、状态、参数和遥测装入该闭环
    ↓
第四步：用Flash、波形、故障和长稳证明它可交付
```

## 4. 背景与问题

当前工程已经完成以下基础建设：

- App、Services、Drivers、BSP边界；
- SCI ISR→SPSC队列→主循环的数据所有权模型；
- 100 μs时基和1/10/100 ms调度；
- 非阻塞SPI桥接；
- 模块Context和诊断快照；
- RAM Debug和Flash Release构建；
- 39项Host测试和板上通信回归；
- 内存、算法、构建、硬件与时序文档。

但它仍存在以下平台化缺口：

- 工程名仍绑定旧通信功能与重构阶段；
- 编译器工程元数据仍存在6.2.7→25.11.0.LTS版本提示；
- 活动源码树仍保留旧 `firmware/board`、`firmware/comm` 参考实现；
- 尚无ADC、ePWM和Trip Zone平台驱动；
- 尚无快速控制路径、统一安全状态和故障管理；
- 尚无运行参数原子切换和遥测双缓冲；
- Flash断电启动、WCET、波形、故障注入和长稳验证尚未完成。

## 5. 产品目标

### 5.1 核心目标

1. 将当前工程正式转化为名称、结构和文档一致的F28335实时控制平台。
2. 建立 `ePWM触发ADC→控制计算→PWM影子更新` 的确定性快速链路。
3. 建立PWM默认关闭、硬件Trip Zone优先、软件故障统一收口的安全模型。
4. 建立与硬件无关、可在PC上测试的Control和Algorithm接口。
5. 建立运行参数原子切换、遥测快照和多速率任务框架。
6. 用构建、Host测试、板上测试、WCET和长稳数据证明平台可用性。
7. 通过Prototype与Industrial两个档位，在快速样机开发和工业交付之间复用同一架构与代码。

### 5.2 成功指标

- 工程名称、路径、产物和文档中不再使用 `DROOP_SPI_UART_REFACTOR` 作为平台身份；历史记录除外。
- RAM Debug与Flash Release均为0 error、0 warning。
- 四个标准档位/构建组合均能生成名称明确、配置可追溯的产物。
- ADC采样与PWM周期同步，无随机软件触发抖动。
- 快速控制ISR最坏执行时间目标不超过控制周期的20%；最终以实测为准。
- Trip Zone可以不依赖软件立即把PWM置为安全状态。
- 控制算法不包含TI设备头文件，Host测试可独立运行。
- 参数更新不会在控制周期中间产生半新半旧状态。
- 通信、调度、控制和诊断在压力测试下无队列踩踏或未解释的数据丢失。
- Flash断电独立启动、故障注入和8～24小时长稳最终通过。

## 6. 非目标

本PRD第一版不要求：

- 一次性实现所有电源拓扑和控制算法；
- 引入RTOS；
- 建立通用到所有C2000器件的跨芯片HAL；
- 立即实现Bootloader或现场固件升级；
- 在未确认功率级安全条件时闭环驱动真实功率回路；
- 为尚不存在的功能创建空实现和虚假测试结果。
- 为Prototype与Industrial复制两套长期分叉的源码。

## 7. 用户与使用场景

### 7.1 主要用户

- 需要开发F28335电源控制固件的嵌入式工程师；
- 需要编写PID、滤波、下垂控制等算法的控制工程师；
- 需要通过SCI/SPI/CAN观察和修改运行参数的联调人员；
- 需要复现构建、烧写和故障的维护人员。

### 7.2 核心使用场景

1. 工程师选择ADC通道和PWM周期，在BSP中完成板级配置。
2. ePWM硬件触发ADC，ADC ISR读取采样并调用快速控制入口。
3. 控制模块输出占空比或相位，PWM驱动在周期边界通过影子寄存器更新。
4. Trip Zone在过流等紧急条件下立即关闭PWM。
5. 1/10/100 ms任务执行状态机、慢速外环、故障去抖、诊断和通信。
6. 通信服务提交待更新参数，系统在安全周期边界校验并切换生效。
7. Host测试验证算法和状态机，硬件测试验证时序、保护和长稳。

## 8. 总体架构

```text
App / Composition Root
├─ ISR adapters ───────→ Drivers ───────→ TI registers
├─ Scheduler ──────────→ Services
├─ Fast control path ──→ Control ───────→ Algorithm
└─ Board initialization→ BSP ───────────→ Drivers

Services
├─ Communication
├─ Parameter manager
├─ Telemetry
└─ Diagnostics
```

快速路径：

```text
ePWM SOCA
   ↓
ADC conversion
   ↓
ADC ISR
   ├─ DrvAdc_ReadSamples()
   ├─ Control_FastStep()
   ├─ DrvEpwm_ApplyShadow()
   └─ clear flags / PIE ACK
```

后台路径：

```text
1 ms   → 快速后台服务、参数提交检查
10 ms  → 系统状态、慢速外环、故障去抖
100 ms → 遥测、LED、维护和低速通信
```

## 9. 功能需求

### FR-01 平台身份与构建

- 工程、产物和文档统一使用 `F28335_RTControl_Platform`。
- 支持RAM Debug和Flash Release。
- 固定并记录受支持的CCS/C2000编译器版本。
- 生成物不得进入源码提交。
- 旧参考代码不得继续位于活动源码搜索路径。

### FR-02 ADC采样

- 新增 `drv_adc.c/.h`，只有驱动层访问ADC寄存器。
- 支持通道、采样窗口、触发源和序列配置。
- 首个版本使用ePWM硬件触发ADC。
- 提供有界、无阻塞的采样读取接口。
- 明确原始ADC码、校准值和工程量之间的转换边界。
- 为未来DMA采样保留接口，但第一版不强制启用DMA。

### FR-03 ePWM输出

- 新增 `drv_epwm.c/.h`。
- 支持周期、计数模式、比较值、影子寄存器和死区配置。
- 默认上电状态必须禁止有效功率输出。
- 占空比、相位和比较值必须限幅。
- PWM更新必须在确定的周期边界生效。

### FR-04 硬件保护

- 支持ePWM Trip Zone配置、触发、锁存、查询和受控清除。
- 严重故障发生时不依赖主循环或通信即可关闭PWM。
- 清除Trip前必须满足明确的恢复条件。
- 保留首个故障原因和故障发生时的关键快照。

### FR-05 快速控制接口

- 新增 `firmware/control`。
- 使用Context持有控制状态，不使用外部可写全局变量。
- 最低接口为：

```c
void Control_FastStep(ControlContext *context,
                      const ControlInput *input,
                      ControlOutput *output);
```

- Input、Output和Context边界必须明确。
- 快速控制函数不访问寄存器、不读取全局tick、不调用delay和通信服务。
- 快速控制代码根据WCET实测决定是否放入 `fastcode`。

### FR-06 算法层

- 新增实际需要的PID、滤波、坐标变换或下垂算法，不创建无业务价值的空模块。
- Algorithm不包含TI设备头文件。
- Algorithm不访问寄存器、ISR、全局tick或动态内存。
- 每个算法必须有Host边界、饱和、复位和异常输入测试。

### FR-07 系统状态与故障管理

- 建立以下最小系统状态：

```text
BOOT → INIT → STANDBY → RUN
                    ↘ FAULT
```

- 提供统一入口：

```c
void System_EnterFault(SystemFault fault);
```

- 进入FAULT必须使控制输出失效，并请求PWM进入安全状态。
- 故障分为硬件快速故障、软件控制故障和通信/维护故障。
- 恢复策略必须按故障类型明确区分自动恢复、人工确认和禁止恢复。

### FR-08 参数管理

- 通信不得直接修改活动控制参数。
- 参数更新采用 `pending→校验→周期边界提交→active` 模型。
- 参数结构包含版本、范围和有效性检查。
- 若未来存入Flash，必须增加CRC和掉电一致性设计。

### FR-09 遥测与诊断

- 快速ISR只写轻量、固定成本的遥测快照。
- 后台通过双缓冲或版本快照读取稳定数据。
- 诊断包含ISR WCET、Scheduler miss、ADC异常、Trip、参数拒绝和通信统计。
- 诊断读取不得改变诊断值。

### FR-10 通信扩展

- 保留当前SCI→SPI参考应用作为回归负载。
- 新通信协议放入Services，寄存器访问放入Drivers。
- CAN/I2C等新外设必须遵守ISR→队列→主循环协议处理模型；硬实时控制数据除外。
- 通信拥塞不得阻塞快速控制ISR。

### FR-11 双档位配置

- 使用唯一的公共定义声明当前产品档位，例如：

```c
typedef enum
{
    PLATFORM_PROFILE_PROTOTYPE = 0,
    PLATFORM_PROFILE_INDUSTRIAL = 1
} PlatformProfile;
```

- 实际档位在编译期确定，生产二进制不得通过通信命令切换为Prototype。
- 档位配置集中存放，不允许在各模块散落大量 `#ifdef INDUSTRIAL`。
- 推荐由 `platform_profile.h` 生成统一的能力宏和常量，再由模块读取配置。
- 二进制必须暴露档位、版本、构建时间、Git提交和配置CRC等身份信息。
- Host测试必须分别编译并验证Prototype和Industrial配置。
- RAM/Flash产物名必须包含产品档位，避免把样机固件误当成工业发布版本。

Prototype档要求：

- 支持调试器暂停和丰富诊断；
- 允许使用编译期开发默认参数；
- 可将非关键外设缺失降级为警告；
- 支持安全的开环、固定占空比、ADC观测和算法旁路测试模式；
- 故障原因必须保留，但可在满足安全条件时允许人工快速复位；
- 不得被标记为生产发布版本。

Industrial档要求：

- 启动时验证配置版本、范围和CRC；
- 关键外设、自检或校准失败时失效关闭；
- Watchdog策略启用并经过故障注入验证；
- 严重故障锁存，恢复必须满足明确条件；
- 禁止调试旁路、未授权固定占空比和未经校验的参数写入；
- 编译警告为0，静态检查、WCET、Flash启动和长稳门槛必须通过；
- 生成正式版本号、发布清单和可追溯标签。

## 10. 非功能需求

### NFR-01 实时性

- 所有ISR必须有明确截止时间和WCET记录。
- 快速控制ISR不得执行格式化输出、阻塞等待和动态分配。
- Scheduler事件合并和miss必须可观测。
- 计时计算必须覆盖32位tick回绕。

### NFR-02 安全性

- PWM默认关闭。
- 初始化失败不得进入RUN。
- 参数无效不得影响活动控制参数。
- 紧急保护优先使用硬件Trip Zone。
- 故障清除不能自动重新使能功率输出，除非需求明确允许。
- Prototype和Industrial必须共享同一套最低功率安全线；任何档位差异都不得绕过PWM默认关闭、限幅和Trip Zone。

### NFR-03 可测试性

- Algorithm、Control状态机、参数管理和协议逻辑必须可Host测试。
- Driver通过板上测试验证。
- 每次变更必须运行Host测试、RAM构建和Flash构建。
- 控制链必须具备开环、限幅、Trip和故障恢复测试模式。

### NFR-04 可维护性

- 只有Drivers和TI平台代码直接访问寄存器。
- BSP只保存板级配置和初始化编排。
- 每个共享字段必须有唯一写入者。
- 不建立巨型公开 `g_sSystemParam`。
- 编译目标为0 error、0 warning。

### NFR-05 可追溯性

- 每个阶段保留map、构建日志、Host结果和硬件记录。
- 未验证项目必须显式标记，不得用估算代替实测。
- 失败回归优先使用Git revert和最小范围修复。

## 11. 四步实施路线（详细）

# 第一步：平台身份与工程洁净化

### 目标

把当前重构工程正式转换为名称、构建和文档一致的平台工程，不改变硬件行为。

### 工作内容

- 将目录、CCS项目和构建产物统一改名为 `F28335_RTControl_Platform`。
- 新增集中式 `platform_profile.h`，定义Prototype/Industrial档位和合法组合。
- 建立四个标准构建身份：Prototype RAM/Flash、Industrial RAM/Flash。
- 确保产物名、版本信息和诊断快照可以区分档位。
- 更新 `.project`、Release脚本、文档路径和硬件测试DUT名称。
- 将编译器元数据迁移到当前受支持的25.11.0.LTS，消除版本警告。
- 将旧 `firmware/board`、`firmware/comm` 移至 `reference/legacy` 或从活动工程删除。
- 创建根目录 `README.md`，说明平台定位、构建、测试和当前状态。
- 增加一键质量门脚本：Host测试、静态边界检查、RAM构建、Flash构建、warning检查。
- 保留当前UART→SPI行为和测试基线。

### 验收门

- 工程路径、CCS名称、产物和文档统一使用新名称。
- Host测试全部通过。
- RAM Debug与Flash Release均0 error、0 warning。
- JTAG下的1/64/65字节回归与改名前一致。
- Prototype与Industrial配置均通过Host编译和静态边界检查。
- 不存在可由运行时通信命令把Industrial切换为Prototype的路径。
- map中的关键段地址和大小变化均有解释。
- 活动源码树无旧实现重复文件。

# 第二步：ADC/ePWM/Trip Zone硬件底座

### 目标

建立最小、安全、可测量的 `PWM触发ADC→ISR读取→PWM影子更新` 垂直链路。

### 工作内容

- 实现 `drv_adc`、`drv_epwm` 和必要的中断绑定。
- 在BSP中定义ADC通道、采样窗口、PWM频率、死区和引脚。
- ePWM产生ADC硬件触发，不使用主循环软件触发。
- ADC ISR读取采样，执行安全的比例/直通测试逻辑，更新PWM影子寄存器。
- 建立Trip Zone默认安全状态、触发和受控清除。
- Prototype提供安全的开环/固定输出调试入口；Industrial构建必须禁用这些旁路入口。
- 增加ADC原始值、PWM比较值、Trip和ISR WCET诊断。
- 使用低风险负载或禁止功率输出模式完成首次验证。

### 验收门

- PWM上电默认关闭，未进入RUN前无有效功率输出。
- PWM频率、死区、触发点和ADC采样点符合配置。
- ADC ISR与PWM周期同步，无软件轮询触发。
- 占空比和比较值限幅正确。
- Trip Zone可独立于软件关闭PWM并锁存原因。
- 两个产品档位的PWM默认关闭、限幅和Trip行为完全一致。
- 快速ISR WCET已测量并满足目标预算。
- 现有通信与Scheduler回归全部通过。

# 第三步：控制、状态、参数与遥测平台

### 目标

在硬件垂直链路上建立可复用的控制软件模型，使算法、参数、故障和通信互不踩踏。

### 工作内容

- 实现 `ControlInput`、`ControlOutput`、`ControlContext` 和 `Control_FastStep`。
- 建立BOOT、INIT、STANDBY、RUN、FAULT状态机。
- 实现统一故障管理和PWM安全关闭路径。
- 对Prototype和Industrial分别实现故障恢复策略，但共享故障分类和安全关断路径。
- 实现参数 `pending/active` 校验和周期边界提交。
- 实现遥测双缓冲或版本化快照。
- 按实际需求加入首个PID/滤波/下垂算法及Host测试。
- 将慢速外环、故障去抖和参数提交放到1/10/100 ms任务。
- 增加算法限幅、复位、异常输入和状态迁移测试。

### 验收门

- Control与Algorithm均不包含TI设备头文件。
- 算法Host测试覆盖正常、边界、饱和、复位和异常输入。
- 通信更新参数时，快速控制只能看到完整的旧版本或新版本。
- 所有RUN→FAULT路径都能使PWM进入安全状态。
- Industrial拒绝所有仅Prototype允许的调试旁路和未授权参数提交。
- 遥测读取不存在半新半旧数据。
- 快速控制ISR满足WCET，Scheduler无未解释miss。
- UART/SPI参考应用和现有回归继续通过。

# 第四步：生产验证与可复用发布

### 目标

完成Flash、实时性、保护、长稳和文档证据，使平台可以作为后续控制项目的可信起点。

### 工作内容

- 完成Flash实际烧写、断开JTAG和断电独立启动。
- 测量ADC ISR、Timer ISR、SCI ISR、主循环和周期任务WCET/抖动。
- 用示波器或逻辑分析仪测量PWM、ADC触发、ISR标记和通信波形。
- 执行Trip Zone、SPI超时、MISO异常和参数拒绝故障注入。
- 运行8～24小时长稳测试。
- 建立最终测试矩阵、发布清单和已知限制。
- 建立正式版本号、Git标签和可重复构建产物。
- 分别建立Prototype演示发布清单与Industrial正式发布清单，禁止混用。
- 评估是否从当前工程派生“平台模板”和“具体产品应用”两个仓库/目录。

### 验收门

- Flash断电独立启动通过。
- 所有关键ISR有真实WCET和裕量记录。
- PWM、ADC触发、控制更新和Trip波形符合设计。
- 故障注入结果与安全策略一致。
- 8～24小时运行无死锁、内存踩踏和未解释错误计数增长。
- 新工程师可依据README和文档完成构建、烧写、测试和问题定位。
- 发布标签明确区分平台版本与具体产品版本。
- Industrial_Flash_Release满足全部工业验收门；Prototype产物带有清晰的非生产标识。

## 12. 需求优先级

### Must

- 工程正式改名并消除工具链警告；
- Prototype/Industrial双档位及其构建身份；
- ADC/ePWM同步链路；
- PWM默认关闭和Trip Zone保护；
- Control/Algorithm纯接口；
- 系统状态和统一故障收口；
- 参数原子提交；
- RAM/Flash构建、Host测试和硬件回归。

### Should

- 遥测双缓冲；
- ADC DMA扩展能力；
- CAN等新通信服务；
- 自动质量门脚本；
- 实机WCET与长稳自动记录。

### Could

- Bootloader；
- 参数Flash持久化；
- 自动生成板级配置；
- 平台模板工程和产品工程分仓；
- HIL自动化测试。

## 13. 风险与对策

| 风险 | 影响 | 对策 |
|---|---|---|
| 改名遗漏硬编码路径 | Release构建或CCS导入失败 | 全仓搜索旧名称，RAM/Flash双构建验证 |
| 编译器升级改变代码生成 | 时序或浮点结果变化 | 独立提交、map对比、Host与硬件全回归 |
| PWM首次接入误驱动功率级 | 硬件损坏 | 默认关闭、低压/空载、Trip先于闭环 |
| ADC采样点错误 | 控制噪声或失稳 | ePWM硬件触发、示波器验证采样时刻 |
| ISR计算超期 | 控制抖动或丢中断 | WCET预算、fastcode、减少回调和诊断开销 |
| 参数更新并发 | 控制输出突变 | pending/active周期边界提交 |
| 通信压力影响控制 | 实时性下降 | 控制ISR优先、后台队列、限流和drop诊断 |
| 故障恢复过于宽松 | PWM意外重启 | 故障分级、人工确认、受控清Trip |

## 14. 最终完成定义

只有以下两级状态分别满足，才可声明对应完成：

### 平台软件能力完成

- 新名称、构建、目录和文档一致；
- ADC/ePWM/Trip Zone、Control、Algorithm、状态、参数和遥测边界形成；
- Host测试、RAM构建、Flash构建和基础JTAG回归通过；
- 无直接上层寄存器访问、无公开可写业务全局变量、无阻塞主循环。

### 平台生产验证完成

- Flash断电启动、WCET、波形、故障注入和长稳全部有实测证据；
- 功率输出默认安全，所有严重故障都能关闭PWM；
- 最终测试矩阵通过，未验证项为零或经正式风险接受；
- 发布产物、版本、标签和文档可重复获取。
