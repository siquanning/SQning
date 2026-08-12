# 错误记录

> 本项目的 `.learnings` 记录统一使用中文，命令、信号名、文件名和工具原始报错除外。

## [ERR-20260812-001] Quartus 13 SignalTap界面异常

**记录时间**：2026-08-12T11:20:00+08:00
**优先级**：中
**状态**：已解决
**领域**：配置

### 摘要
从Quartus II 13.0 SP1打开SignalTap后留下无窗口后台进程，并产生多个内部错误窗口。

### 原始错误

```text
Quartus II Internal Error
quartus_stpw.exe processes remained running without a targetable window.
```

### 发生环境
- Windows上的Quartus II 13.0 SP1。
- 从 `Tools > SignalTap II Logic Analyzer` 打开，并尝试携带工程参数启动。
- 多个 `quartus_stpw.exe` 进程没有主窗口，随后Quartus启动产生内部错误。
- 命令行 `quartus_stp.exe -t` 的JTAG接口仍然可靠。

### 解决办法
紧急硬件排查不依赖Quartus 13 SignalTap界面，改用 `quartus_stp.exe -t <脚本>`、`::quartus::jtag` 和IEEE 1149.1 SAMPLE。使用以下命令生成器件BSDL：

```text
quartus_eda --bsdl EP4CE10F17C8 -output_directory <目录>
```

清理无窗口Quartus进程前，先确认没有编译或烧录子进程运行。

### 元数据
- 可复现性：未知
- 相关文件：`debug_jtag.tcl`、`EP4CE10F17C8_pre.bsd`

### 解决结果
- **解决时间**：2026-08-12T11:17:23+08:00
- **说明**：绕过界面，通过Quartus JTAG Tcl接口完成诊断。

---

## [ERR-20260812-002] H2滤波修复导致H1回归

**记录时间**：2026-08-12T11:25:00+08:00
**优先级**：严重
**状态**：已解决
**领域**：配置

### 摘要
为修复H2换相缺口而加入的滤波器导致H1 a1被固定为低。

### 原始表现

```text
Quartus编译：0 errors，内部时序通过。
硬件结果：gates_out[0] / B5 始终为低。
JTAG SAMPLE：B5_PAD ones=0 transitions=0；B5_CORE ones=0 transitions=0。
```

### 发生环境
- 在 `CPLD_pwm_phase.vhd` 内直接增加300周期全低计数器和活动状态寄存器。
- 编译和时序通过后，只检查构建结果便立即烧入。
- 用户随后从外部波形发现H1 a1消失。
- JTAG证明A3/A5严格互补，故障位于FPGA内部B5输出通路。
- `deadtime_gen` RTL仿真正负输出均正常；恢复旧相模块后B5立即恢复。

### 解决办法
滤波器不能放在相位PWM模块内部。将其移到 `CPLD_pwm.vhd`，只生成H2使能保护，保持H1透传和死区通路独立；门极修改必须通过边界扫描回归后才能宣布完成。

### 元数据
- 可复现性：是
- 相关文件：`CPLD_pwm.vhd`、`CPLD_pwm_phase.vhd`、`debug_jtag.tcl`、`tb_deadtime_gen.vhd`
- 关联记录：`LRN-20260812-001`

### 解决结果
- **解决时间**：2026-08-12T11:17:23+08:00
- **说明**：滤波器移至H2专用使能；最终B5/B6正常互补，H2 a1保持高电平。

---

## [ERR-20260812-003] 第一次诊断缺少硬件验证

**记录时间**：2026-08-12T11:25:00+08:00
**优先级**：高
**状态**：已解决
**领域**：测试

### 摘要
第一次对H2 a1缺口的原因判断虽然合理，但没有验证便修改并烧入门极逻辑。

### 原始问题

```text
初步推断：独立同步器在换相时短暂产生 h1_gates="0000"，
立即生效的DSP活动检测造成H2 a1缺口。

缺少的证据：烧入修复前，没有内部采样或完整的相邻门极回归验证。
```

### 解决办法
波形和RTL推理只能作为假设。修改功率门极逻辑前要定义所有相关输出的验收条件，保留回滚SOF，烧入后验证真实器件而不仅是目标信号。

### 元数据
- 可复现性：是
- 相关文件：`CPLD_pwm.vhd`、`CPLD_pwm_phase.vhd`、`debug_jtag.tcl`
- 关联记录：`LRN-20260812-001`、`ERR-20260812-002`

### 解决结果
- **解决时间**：2026-08-12T11:17:23+08:00
- **说明**：建立覆盖H1输入、H1输出和H2输出的JTAG SAMPLE回归，最终版本全部通过。

---

## [ERR-20260812-004] Quartus命令与仿真流程错误

**记录时间**：2026-08-12T11:25:00+08:00
**优先级**：低
**状态**：已解决
**领域**：测试

### 摘要
部分诊断命令因为超时时间过短、Quartus参数顺序错误以及ModelSim工作库映射错误而失败。

### 原始错误

```text
quartus_sh完整编译在短超时下被判定超时，但后台仍在继续。
quartus_eda在参数顺序错误时拒绝 --bsdl 命令。
ModelSim首次运行提示：Library work not found.
```

### 解决办法
- Quartus完整编译预留至少60秒，出现后台任务标识时使用等待机制继续读取。
- Quartus 13正确BSDL命令为：`quartus_eda --bsdl EP4CE10F17C8 -output_directory <目录>`。
- ModelSim编译前先建立并正确映射本地 `work` 库。

### 元数据
- 可复现性：是
- 相关文件：`tb_deadtime_gen.vhd`、`modelsim.ini`、`EP4CE10F17C8_pre.bsd`

### 解决结果
- **解决时间**：2026-08-12T11:17:23+08:00
- **说明**：后续完整编译、BSDL生成和死区RTL仿真均成功完成。

---
