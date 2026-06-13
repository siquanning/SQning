# 2P_DAB — DAB 电源控制系统

基于 TI TMS320F28335 的双有源桥（DAB）电源控制系统。

## 目录结构

```
.
├── app/                   # 应用层（main, 状态机, 软启动, 控制循环）
├── bsp/                   # 板级支持包（系统时钟, GPIO, LED, 延时）
├── control/               # 控制算法（PID, DPS, 调制, IIR 滤波）
├── drivers/               # 外设驱动（ePWM, ADC, SCI）
├── protocols/             # 通信协议（Modbus RTU）
├── protection/            # 保护逻辑（OVP, OCP, TZ）
├── include/               # 共享头文件（config.h）
├── docs/                  # 产品文档（PRD, 里程碑计划）
├── assets/
│   └── reference/         # 参考代码（PLECS 仿真算法）
├── notes/                 # 学习笔记、踩坑记录
├── 28335_RAM_lnk.cmd      # 链接脚本（RAM 调试）
├── .cproject              # CCS 工程配置
├── .projectspec           # CCS 工程描述
└── .syscfg                # SysConfig 引脚配置
```

## 开发环境

- **IDE**: TI Code Composer Studio (CCS)
- **编译器**: TI C2000 Compiler
- **配置工具**: SysConfig
- **调试工具**: CCS Debug Probe
