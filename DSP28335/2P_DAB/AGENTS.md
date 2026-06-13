# AGENTS.md — 2P_DAB AI 开发助手指南

## 项目概述

基于 TI TMS320F28335 (C2000 DSP) 的 DAB（双有源桥）电源控制系统。目标：8 路 PWM 门极驱动、DPS 调制、闭环稳压、Modbus 通信。

## 开发环境

| 组件 | 路径/版本 |
|------|----------|
| CCS IDE | E:\ti\ccs2051 (20.5.1) |
| 编译器 | TI C2000 v22.6.1.LTS，COFF ABI，FPU32 |
| C2000Ware | E:\repos\DSP28335\C2000\C2000Ware_5_04_00_00 (5.04.01.00) |
| SysConfig | 1.21.0（集成在 CCS IDE 内） |
| 链接脚本 | firmware/28335_RAM_lnk.cmd（RAM 调试），DSP2833x_Libraries/F28335.cmd（Flash） |

## 文件夹结构

| 文件夹 | 用途 |
|--------|------|
| `firmware/app/` | 应用层：main()、状态机调度 |
| `firmware/bsp/` | 板级支持包：时钟、GPIO、delay |
| `firmware/control/` | 控制算法：PID、DPS 调制、软启动、IIR |
| `firmware/drivers/` | 外设驱动：ePWM、ADC、SCI、TZ |
| `firmware/include/` | 公共头文件：common.h、全局类型 |
| `firmware/protection/` | 保护逻辑：过压/过流检测、故障锁存 |
| `firmware/protocols/` | 通信协议：Modbus RTU Slave |
| `DSP2833x_Libraries/` | TI 外设库（SysCtrl/Pie/Gpio/DefaultIsr/USDelay 等） |
| `docs/` | 产品文档：PRD、里程碑记录 |
| `assets/` | 参考资料：原理图、参考算法 |
| `notes/` | 开发笔记、踩坑记录 |

## Include 路径

从 `.cproject` 提取，编译器 `-I` 顺序：

1. `C2000Ware/device_support/f2833x/common/include` — EPwm_defines.h, GlobalPrototypes.h
2. `C2000Ware/device_support/f2833x/headers/include` — DSP2833x_Device.h, 寄存器定义
3. `${CG_TOOL_ROOT}/include` — 编译器内置头文件
4. `firmware/` — 项目本地头文件（`#include "drivers/epwm.h"` 对应 `firmware/drivers/epwm.h`）

所有源码 include 基于 firmware/ 根。公共头 `common.h` 已引入 `DSP2833x_Device.h` + `DSP2833x_GlobalPrototypes.h`。

## 技术参数速查

| 参数 | 值 | 来源 |
|------|-----|------|
| SYSCLKOUT | 150MHz | PLL ×10/2, XTAL 30MHz |
| PWM 频率 | 10kHz | TBPRD = 7500, 递增-递减 |
| 死区时间 | 200ns | DBRED = DBFED = 30 ticks @150MHz TBCLK |
| ePWM1-4 引脚 | GPIO00-07 | ePWM1A/B → GPIO00/01, … → GPIO06/07 |
| ADC 触发 | ePWM1 SOCA, 周期中点 | 避开开关噪声 |
| 控制频率 | 1kHz | Systick 1ms |
| SCI-A | GPIO35/36 | Modbus RTU, 9600-8N1 |
| TZ1 | GPIO12 | 硬件故障封波 |
| LED1-5 | GPIO64-68 | 心跳/通信/故障/保留/状态 |

## 开发规则

### 1. 代码修改

- **只改需要的**：不改相邻代码、不改注释、不改格式。除非你的修改让它变成死代码。
- **include 路径**：新增 `.c/.h` 放入 `firmware/` 子目录后，`#include` 基于 `firmware/` 根（如 `#include "drivers/epwm.h"`）。
- **寄存器操作**：EALLOW 保护的寄存器必须在 EALLOW/EDIS 之间操作。
- **新文件**：`.c` 文件放在 `firmware/` 对应子目录下，会被 `.cproject` 的递归 sourcePath 自动编译（无需手动改工程文件）。
- **注释语言**：所有代码注释必须使用**中文**。头文件、`.c` 文件、宏定义注释一律中文。

### 2. SysConfig (`.syscfg`)

**禁止直接编辑 `.syscfg` 文件。** 修改引脚/外设配置必须通过以下方式之一：
- **优先**：CCS IDE → SysConfig GUI 编辑器
- **备选**：SysConfig MCP 工具（如果已配置 MCP 服务器）

GPIO 和外设的寄存器初始化写在 C 代码里（如 `epwm_gpio_init()`）是允许的，这不涉及 `.syscfg`。

### 3. 构建与烧录

- **构建**：在 CCS IDE 中 Project → Build Project，或点锤子图标
- **烧录/调试**：CCS IDE → Debug（自动烧录 `.out` 到 RAM）
- **编译错误定位**：看 CCS Console 输出；错误格式为 `文件 "路径", 行号: 错误描述`

### 4. 文档集中

- 设计文档放 `docs/`，笔记放 `notes/`，参考图放 `assets/`
- 代码区（`firmware/`、`DSP2833x_Libraries/`）不引用文档区文件，代码自包含

### 5. 寄存器操作规范

```c
// 正确：EALLOW 保护的寄存器
EALLOW;
SysCtrlRegs.PCLKCR1.bit.EPWM1ENCLK = 1;
EDIS;

// 正确：非保护的寄存器，直接操作
EPwm1Regs.TBPRD = 7500;

// 正确：联合体访问（如 TBPHS 是 32-bit union）
EPwm1Regs.TBPHS.half.TBPHS = 0;
```

### 6. 已知注意事项

- **时钟初始化**：`clock_init()` 在 `bsp/clock.c` 中，只做 PLL 配置。各外设驱动自行使能各自的 PCLKCR 时钟位。
- **DSP2833x_GlobalVariableDefs.c**：定义了所有外设寄存器变量（EPwm1Regs 等），不能删除。
- **DSP2833x_Headers_nonBIOS.cmd**：将寄存器变量映射到外设帧地址，链接时必须包含。
- **RAM 调试**：当前使用 `28335_RAM_lnk.cmd`，程序加载到 RAM。断电丢失。
- **Watchdog**：`clock_init()` 中已禁用。
- **ADC 跳线帽（重要！）**：PZ-DSP28335-L 板上 ADCINA0 必须用跳线帽短接 P8 端子的 ADCINA0 ↔ ADCA0_S，否则 ADC 读到的是悬空噪声（~729 计数）。板载电位器电压经 1.5k+10k 分压后从 ADCA0_S 输出。
- **ADC 零点校准**：`adc_init()` 设 `zero_offset=0xFFFF`（哨兵），ISR 前 32 次调用自动采集 ePWM 触发样本取平均作为噪声偏移。功率级必须在上电前完成校准（V2=0）。

### 7. 保护逻辑 (Phase 7)

**两层保护**：
- **硬件层**：GPIO12→TZ1，ePWM OST 模式。TZ1 拉低时硬件直接关断 PWM（<100ns），不经过 CPU，代码跑飞也能关断。
- **软件层**：`protection_step()` 每 1ms 被 `control_step()` 调用，检查 OVP（V2>120V 可配）和 OCP（I>15A 可配），3 次连续超阈值后通过 TZFRC 软件强制关断。

**设计决策**：
- **3 次连续** → 抗开关噪声尖峰（单次尖峰持续 <2ms，真实故障持续存在）
- **OST 而非 CBC** → 故障必须锁存，不允许自动恢复（反复开关会损坏 MOSFET）
- **Modbus 写清除** → 安全考虑：确认故障排除后远程清除，入口 `g_prot_fault_clear=1`（第 8 片实现）

**CCS 调试接口**：

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `g_ovp_threshold` | 120.0f | 过压阈值 (V)，调试器可改 |
| `g_ocp_threshold` | 15.0f | 过流阈值 (A) |
| `g_prot_fault` | 0 | 故障锁存状态（1=已锁存，只读） |
| `g_prot_fault_clear` | 0 | 设 1 清除故障（Modbus 入口，写后自清零） |
| `g_ovp_count` / `g_ocp_count` | 0 | 连续超阈值计数（调试用） |
| `g_i_measured` | 0.0f | 电流实测值（待传感器接入，调试器可写值测试 OCP） |

**故障触发后行为**：PWM 全部封锁（TZFLG.OST=1），LED3 常亮，状态机→STATE_FAULT，LED5 快闪。
**故障清除后**：TZ 锁存解除，回 STATE_IDLE，LED3 灭，LED5 灭。`g_start_cmd=1` 可重新启动。

**验证记录 (2026-05-22)**：TZ1 寄存器配置正确（GPIO12 MUX/输入/上拉，4路 ePWM OSHT1=1 & FORCE_LO）。OVP 功能通过（设阈值 -1.0 验证 0V→触发，TZFLG.OST=1 全部封锁，清除回 IDLE 正常）。

**已修复的坑**：
- `TZFRC` 是 EALLOW 保护寄存器，直接写会被静默忽略 → 必须 `EALLOW`/`EDIS` 包裹
- `state_machine_step` 原来要求非 IDLE 才响应 `g_fault_flag`，导致 IDLE 下保护触发无法进入 FAULT → `protection_step` 直接设 `g_system_state = STATE_FAULT`
- 已处 FAULT 时每周期重置 `led5_tick=0` 导致 LED5 死锁不闪 → 条件追加 `&& g_system_state != STATE_FAULT`

## 模块依赖

```
app ──→ control ──→ drivers
  │        │
  ├─→ protection ──→ drivers
  │        │
  ├─→ protocols ──→ drivers
  │
  └─→ bsp
```

- `app` 是顶层调度者，依赖所有下层
- `control` 依赖 `bsp`（systick）和 `drivers`（ADC/PWM）
- `protection` 独立运行，直接读 ADC 并控制 TZ
- `protocols` 依赖 `drivers`（SCI）
- `bsp` 为最底层，提供时钟和延时
- `include/common.h` 被所有模块引用

### 8. Modbus RTU 通信 (Phase 8)

**实现位置**：`DSP2833x_DAB/` CCS 工程（独立于 `firmware/`），从 `DSP2833x_EPWM_modbus` 参考工程移植。

**文件清单**：

| 文件 | 作用 |
|------|------|
| `DSP2833x_DAB/APP/common/types.h` | Uint8 typedef（TI COFF ABI 无 uint8_t） |
| `DSP2833x_DAB/APP/modbus/modbus_slave.h/.c` | Modbus RTU Slave 协议栈（03/04/06，CRC-16 0xA001） |
| `DSP2833x_DAB/APP/modbus/app_modbus.h/.c` | DAB 寄存器桥接 + 范围校验 |
| `DSP2833x_DAB/APP/sci/sci_driver.h/.c` | SCI-A 驱动（GPIO35/36, 9600-8N1, FIFO中断接收） |
| `DSP2833x_DAB/APP/gpio/gpio_config.c` | GPIO35/36 初始化 |
| `DSP2833x_DAB/User/main.c` | 主循环：MB_Poll 驱动 LED2，1kHz systick |

**通信参数**：SCI-A (GPIO35/36)，9600-8-N-1，RTU Slave addr=1，5ms 帧间超时。

**寄存器映射**：

| 地址 | 名称 | 范围 | 默认 | 说明 |
|------|------|------|------|------|
| 40001 | V2_ref | 0–2000 | 0 | 输出电压给定 (×0.1V) |
| 40002 | SoftStartTime | 10–5000 | 100 | 软启动时间 (ms) |
| 40003 | PID_Kp | 0–10000 | 1000 | 比例系数 (×1000) |
| 40004 | PID_Ki | 0–10000 | 100 | 积分系数 (×1000) |
| 40005 | PID_Kd | 0–10000 | 1000 | 微分系数 (×1000) |
| 40006 | Command | 0–2 | 0 | 0=停机, 1=启动, 2=清除故障 |
| 40007 | OVP_Threshold | 0–3000 | 1200 | 过压阈值 (×0.1V) |
| 40008 | OCP_Threshold | 0–500 | 150 | 过流阈值 (×0.1A) |
| 30001 | V2_Actual | 0–2000 | — | 输出电压 (×0.1V, R/O) |
| 30002 | Power | 0–65535 | — | 功率 (W×10, R/O) |
| 30003 | D1 | 0–1000 | — | D1 移相角 (×1000, R/O) |
| 30004 | D2 | 0–1000 | — | D2 移相角 (×1000, R/O) |
| 30005 | State | 0–3 | — | 状态 (R/O) |
| 30006 | FaultCode | 0–2 | — | 故障码 (R/O) |

**与参考工程的关键差异**（均为有意改进）：
- `MB_Poll()` 返回 Uint16（参考 void）→ 驱动 LED2 通信指示
- `MB_ApplyRegChanges(Uint16 reg_addr)` 带参数（参考无参）→ 按寄存器地址校验范围
- 已删除参考工程中未使用的全局变量 `Scia_Received_Data` / `Scia_Data_Received_Flag`

**已踩坑（重要）**：
1. **COFF ABI 无 `<stdbool.h>`**：不能用 `bool`/`true`/`false`，必须用 `Uint16` 的 0/1
2. **`Uint8` 不存在**：TI 头文件只定义 `Uint16`/`Uint32`，需 `types.h` 补充 `typedef unsigned char Uint8`
3. **GPIO 引脚**：SCI-A 是 GPIO35/36（不是 28/29）。参考工程 DSP2833x_EPWM_modbus 用的就是 35/36
4. **`.cproject` 是唯一真相源**：手动改 makefile 或 subdir_vars.mk 会被 CCS 覆盖，必须改 `.cproject`
5. **编译器版本警告无害**：22.6.1.LTS → 25.11.0.LTS 自动兼容

**下一步 Phase 9**：Modbus 联调 — 用 ModbusPoll 等串口工具读写寄存器，验证远程启停/改参/读状态。

## 里程碑进度

详见 [docs/MILESTONES.md](docs/MILESTONES.md)。当前：第 8 片已完成（Modbus RTU 通信，编译通过，C/H 文件全部检查无问题）。下一片：第 9 片 Modbus 联调 + 系统整定。
