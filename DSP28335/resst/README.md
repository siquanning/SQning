# F28335 Real-Time Control Platform

面向 TMS320F28335 的可复用实时控制固件底座。当前承载的参考应用为 **UART→SPI 桥接**（SCI-A 接收变长帧，经 SPI-A 逐字节转发至 CPLD）。

## 快速开始

### Host 测试（无需硬件，需 Visual Studio 2022）

```bat
cd tests\host
_all.bat
```

### RAM Debug 构建

CCS IDE: 导入项目 → 右键 → Build Configurations → Set Active → 选择 `Debug` (Prototype_RAM_Debug) 或 `Industrial_RAM` (Industrial_RAM_Debug) → Build Project

### Flash 构建

CCS IDE: 导入项目 → Build Configurations → Set Active → 选择 `Flash_Demo` (Prototype) 或 `Flash_Release` (Industrial) → Build Project

或命令行:
```bat
cd Release
build_release.bat   # 默认 Prototype_Flash_Demo
```
```powershell
cd Release
.\build.ps1 -Profile Industrial   # Industrial_Flash_Release
```

## 工具链

| 组件 | 版本 |
|---|---|
| CCS IDE | 20.5.1 |
| C2000 编译器 | **25.11.0.LTS** (C2000 Code Generation Tools) |
| 目标器件 | TMS320F28335, 150 MHz |
| Host 编译器 | MSVC 2022 (仅测试) |

## 四种构建身份

| 产品档位 | 装载方式 | 构建标识 | 用途 |
|---|---|---|---|
| Prototype | RAM Debug | `Prototype_RAM_Debug` | 快速开发、JTAG 调试、算法联调 |
| Prototype | Flash Demo | `Prototype_Flash_Demo` | 样机独立运行、低压演示 |
| Industrial | RAM Debug | `Industrial_RAM_Debug` | 工业逻辑调试、安全路径验证 |
| Industrial | Flash Release | `Industrial_Flash_Release` | 生产候选版本、正式发布 |

档位通过 `-D PLATFORM_PROFILE_PROTOTYPE` 或 `-D PLATFORM_PROFILE_INDUSTRIAL` 在编译期选择，运行时不可切换。

构建产物名包含档位标识，防止样机固件被误用作工业发布。

## 目录结构

```
F28335_RTControl_Platform/
├── firmware/
│   ├── app/           应用层 (main, 调度器, ISR, 诊断, 队列)
│   ├── bsp/           板级支持包 (板卡初始化编排)
│   ├── drivers/       驱动层 (SCI, SPI, GPIO, Timer, SysCtrl, Interrupt)
│   ├── services/      服务层 (UART帧解析, SPI桥接, 指示灯)
│   └── platform_profile.h  集中式产品档位配置
├── SRC/               TI 库源码 (PIE, SysCtrl, CpuTimers, 启动代码等)
├── INCLUDE/           TI 头文件
├── CMD/               辅助 linker 命令文件
├── linker/            RAM Debug / Flash Release linker 脚本
├── config/            通信配置文件
├── tests/
│   ├── host/          PC 端 Host 测试 (uart_frame, spi_request, spi_bridge, sci_rx_queue)
│   └── hardware/      硬件回归测试记录与基线产物 (历史)
├── tools/             构建与质量门脚本
├── reference/legacy/  旧 DROOP_SPI_UART 参考实现 (已从活动构建排除)
├── docs/              架构、内存布局、算法契约、硬件测试等文档
└── Release/           Flash Release 命令行构建脚本
```

## 已验证项目

- [x] Host 测试 (UART 帧解析、SPI 请求、SPI 桥接、SCI 接收队列)
- [x] RAM Debug 构建 (via CCS)
- [x] Flash Release 构建 (via build.ps1 / build_release.bat with 25.11.0.LTS)
- [x] JTAG 下载与执行 (XDS100v3)
- [x] 1/64/65 字节 UART→SPI 回归 (见 `tests/hardware/BASELINE_TEST_RECORD.md`)

## 待硬件验证项目

- [ ] JTAG 1/64/65 字节回归 (改名后 — 待人工硬件回归)
- [ ] Flash 独立启动 (断开 JTAG, 断电重启)
- [ ] ADC/ePWM/Trip Zone (PRD 第二步)
- [ ] 快速控制 ISR WCET 实测 (PRD 第二步)
- [ ] 故障注入与长稳测试 (PRD 第四步)

## 旧参考代码

原始 `DROOP_SPI_UART` 的板级和通信代码保存在 `reference/legacy/` 目录，已从所有活动构建配置中排除。这些文件仅供理解迁移过程参考，不应重新加入编译路径。

## 文档

| 文档 | 内容 |
|---|---|
| [PRD](docs/PRD_F28335_RTCONTROL_PLATFORM.md) | 产品需求文档 |
| [ARCHITECTURE](docs/ARCHITECTURE.md) | 软件架构说明 |
| [BUILD_AND_FLASH](docs/BUILD_AND_FLASH.md) | 构建与烧写指南 |
| [MEMORY_LAYOUT](docs/MEMORY_LAYOUT.md) | 内存布局说明 |
| [HARDWARE_TEST](docs/HARDWARE_TEST.md) | 硬件测试规程 |
| [BASELINE_TEST_RECORD](tests/hardware/BASELINE_TEST_RECORD.md) | 硬件回归测试记录 |

## 历史名称说明

本平台前身为 `DROOP_SPI_UART_REFACTOR` 工程（源自 `DROOP_SPI_UART`）。改名不影响寄存器配置、运行逻辑、内存布局或硬件时序。历史基线产物和测试记录中保留了旧名称引用，均已显式标注为历史来源。
