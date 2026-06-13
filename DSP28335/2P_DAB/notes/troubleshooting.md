# 2P_DAB 项目构建问题排查记录

## 2026-05-20: 首次构建失败 — 7个问题及修复

### 问题1: assets/reference/ 目录被当作源码编译 (BUILD FAILURE)

**现象:** `assets/reference/dps_cscript_algorithm.c` 编译报 29 个错误。
```
error #60: function call is not allowed in a constant expression
```
**原因:** 该文件是 PLECS 仿真 C-Script 代码，包含 `InputSignal()` 等仿真环境专用函数调用在文件作用域，无法在 C2000 编译器中编译。`.cproject` 的 sourceEntries 包含了整个项目根目录，唯独没有排除 `assets/reference`。

**修复:**
1. `.cproject` 添加排除规则: `|assets/reference`
2. `assets/reference/*.c` 重命名为 `.c.bak`
3. 清理 `Debug/sources.mk`、`Debug/makefile`、`Debug/ccsObjs.opt` 中对 assets/reference 的引用

### 问题2: 过时的根目录 .obj 文件导致符号重复定义 (LINK FAILURE)

**现象:** Linker 收到重复的 object 文件引用 — 根目录的 `../DSP2833x_DefaultIsr.obj` 和库目录的 `./DSP2833x_Libraries/DSP2833x_DefaultIsr.obj` 同时存在。

**原因:** 前期开发中源码曾在根目录直接编译，后来源码移入 `firmware/` 和 `DSP2833x_Libraries/`，但旧的 `.obj` 文件未被清理。`Debug/subdir_vars.mk` 通过 `OBJ_SRCS` 引用了这些过时文件。

**修复:**
1. 删除根目录所有 `.obj` 和 `.d` 文件
2. 从 `Debug/subdir_vars.mk` 清除根级别的 `OBJ_SRCS` 条目
3. 从 `Debug/makefile` 移除根级别 `ORDERED_OBJS` 条目
4. 从 `Debug/sources.mk` 的 `SUBDIRS` 中移除 `.`（根目录）
5. 从 `Debug/makefile` 移除根级别的 `-include subdir_vars.mk` 和 `-include subdir_rules.mk`

### 问题3: PLL 配置错误 — 芯片超频 2x (RUNTIME CRASH)

**现象:** `firmware/bsp/clock.c` 中 `clock_init()` 设置 `DIVSEL=3`（÷1），产生 **300MHz** SYSCLKOUT。F28335 最大 SYSCLKOUT 为 **150MHz**。

```c
// 错误: DIVSEL=3 → SYSCLKOUT = 30MHz × 10 / 1 = 300MHz
SysCtrlRegs.PLLSTS.bit.DIVSEL = 3;
```
**正确值:** `DIVSEL=2`（÷2），SYSCLKOUT = 30MHz × 10 / 2 = 150MHz。

**修复 — `clock_init()` 完整重写:**
- DIVSEL: 3 → 2（150MHz 而非 300MHz）
- 添加 `MCLKSTS` 检查（缺时钟检测）
- 添加 `MCLKOFF=1` 在修改 PLLCR 之前（防止误触发缺时钟检测）
- 添加 `GPIOINENCLK=1`（GPIO 输入时钟使能）

### 问题4: 未初始化 PIE 和中断向量表 (RUNTIME CRASH)

**现象:** 如果有任何外设中断被意外触发，将执行默认 ISR 中的 `ESTOP0`（硬件断点），CPU 停止。

**原因:** `main()` 从未调用 `InitPieCtrl()` 和 `InitPieVectTable()`，所有中断向量指向默认 trap 处理函数。

**修复 — main() 添加初始化序列:**
```c
DINT;               // 初始化期间禁止中断
InitPieCtrl();      // 初始化 PIE 控制寄存器
IER = 0; IFR = 0;   // 清除所有中断使能和标志
InitPieVectTable(); // 初始化 PIE 向量表（指向默认 ISR）
// ... 如果有自定义中断，在这之后映射向量 ...
EINT; ERTM;         // 开启全局中断
```

### 问题5: 缺少函数原型声明 (WARNING)

**现象:** `InitPieCtrl` 和 `InitPieVectTable` 的隐式声明警告。

**原因:** 函数原型在 `DSP2833x_GlobalPrototypes.h` 中，该头文件不在 include 路径中（被 `DSP2833x_Examples.h` 间接包含）。

**修复:** 在 `firmware/include/common.h` 添加 `#include "DSP2833x_GlobalPrototypes.h"`。

### 问题6: 调试启动配置缺失

**现象:** `.theia/launch.json` 为空，CCS 调试器无法找到程序路径和目标配置。

**修复:** 添加调试启动配置，指定：
- `program`: `${workspaceFolder}/Debug/2P_DAB.out`
- `targetConfig`: `${workspaceFolder}/targetConfigs/TMS320F28335.ccxml`
- `stopAt`: `main`

---

## 对比参照项目

参照项目: `E:\repos\DSP28335\DSP2833x_EPWM_modbus`（可正常构建和运行）

## 关键差异总结

| 项目 | EPWM_modbus (正常) | 2P_DAB (修复前) |
|------|-------------------|----------------|
| 源码组织 | APP/epwm/, APP/gpio/ 等 | firmware/app/, firmware/bsp/ |
| 系统初始化 | InitSysCtrl() | 自定义 clock_init() |
| PLL DIVSEL | 2 (÷2 = 150MHz) | **3 (÷1 = 300MHz)** |
| PIE 初始化 | 有 | **无** |
| 调试配置 | 完整 | **空** |
| 非源码文件 | 无 | **assets/reference/ 被编译** |
| Target config | TIXDS100v3_Dot7 + tixds100v2c28x | **TIXDS100v3 + tixds100v3c28x (不兼容)** |

---

## 2026-05-20: 调试器无法读取 .ccxml

### 问题7: Target Configuration 文件与 CCS 不兼容

**现象:** CCS Theia 调试器报 "Unable to read file: TMS320F28335.ccxml"

**原因:** `.ccxml` 文件使用了与工作项目不同的 XML 属性名和驱动/设备引用：

| 配置项 | 工作项目 (正常) | 2P_DAB (修复前) |
|--------|----------------|-----------------|
| XML 属性名 | `XML_version` | `XMLVersion` |
| Connection | `TIXDS100v3_Dot7_Connection.xml` | `TIXDS100v3_Connection.xml` |
| Driver | `tixds100v2c28x.xml` | `tixds100v3c28x.xml` |
| Device | `f28335.xml` | `TMS320F28335.xml` |

**修复:** 将 `.ccxml` 完全对齐到已验证的工作项目配置。

---

## 2026-05-20: 第2片 ePWM 驱动编译失败 — uint16_t 未定义

### 问题8: TI C2000 编译器不支持 `uint16_t`（COFF ABI）

**现象:** epwm.h / epwm.c 编译报 14 个错误：
```
error #20: identifier "uint16_t" is undefined
```

**原因:** TI C2000 编译器在 COFF ABI 下使用自定义类型 `Uint16`（定义在 `DSP2833x_Device.h:123`）：
```c
typedef unsigned int       Uint16;
```

COFF ABI 不提供标准 `<stdint.h>` 的 `uint16_t`。EABI 才有。本项目使用 COFF ABI（`.cproject` 配置：`--abi=coffabi`）。

**修复:** `epwm.h` 和 `epwm.c` 中所有 `uint16_t` → `Uint16`，共 14 处。函数签名类型保持大写 U 风格，与 DSP2833x 库文件一致。

---

## 2026-05-20: PWM 输出后单片机区域有高频啸叫

### 问题9: 10kHz PWM 引起可闻噪声

**现象:** `epwm_init()` 执行后，板子区域听到高频啸叫声。注释掉 `epwm_init()` 后啸叫消失。

**原因:** PWM 开关频率 10kHz 处于人耳可闻范围（20Hz~20kHz）。8 路 PWM 在 GPIO00~07 上同步输出 50% 方波，开关瞬态的高 di/dt 通过底板走线电感、门极驱动走线、或功率级磁性元件（变压器/电感）的磁致伸缩效应耦合出机械振动。

**结论:** 正常现象，PWM 确实在运行。验证方式：
1. 断电 → 注释 `epwm_init()` → 重新上电 → 啸叫消失
2. 示波器量 GPIO00~07 任意一路，确认为 10kHz 方波

后续闭环调制度投入后，PWM 占空比变成动态调制，声音特征会随之变化——这在 DAB 电源开发中很常见，不代表硬件故障。

---

## 2026-05-20: 第2片验证 — 波形异常排查

### 问题10: GPIO2 (ePWM2A) 波形异常 — 直角梯形

**现象:** 8 路 PWM 中 7 路正常（50% 方波），仅 GPIO2 波形异常——上升后缓慢衰减，呈"直角梯形"。

**排查过程:**
1. GPIO3 (ePWM2B) 波形正常 → ePWM2 模块本身配置正确
2. `gpio_init()` 未触碰 GPIO2 → 无软件冲突
3. 所有模块通过同一 `epwm_module_init()` 初始化，参数相同

**推测:** GPIO2 在 PZ-DSP28335-L 底板上接有额外负载（对地电容/电阻分压），RC 充放电导致下降沿变缓。待排查方向：
- 关闭 GPIO2 内部上拉（`GPAPUD.bit.GPIO2 = 1`），排除上拉与底板负载打架
- 在 controlCARD 金手指 vs 底板端子分别测量，定位问题在板卡端还是底板走线端
- 查底板原理图 GPIO2 连接

**当前状态:** 未解决，硬件侧排查中。不影响第3片推进——第3片只用到 ePWM1 SOCA 触发 ADC，GPIO2 问题后续处理。

---

## 2026-05-20: 调试器连接故障

### 问题11: XDS100v3 Error -154 — FTDI 通信失败

**现象:**
```
Error -154 @ 0x0: One of the FTDI driver functions used to write data
returned bad status or an error. (Emulation package 20.5.0.3920)
Unable to determine target status after 20 attempts
```

**原因:** XDS100v3 仿真器 USB 转 JTAG 的 FTDI 芯片通信中断。常见于：上一次调试会话异常退出后残留状态；USB 线缆/端口不稳定；目标板供电波动。

**修复（标准流程）:**
1. 目标板断电 → USB 拔掉仿真器 → 等 5 秒 → 重新上电 → 插 USB → CCS 重连
2. CCS: Run → Debug Configurations → 选中 XDS100v3 → Test Connection 验证
3. 检查底板 JTAG 排线是否松动（若为排线连接而非直插）
4. 如果反复出现，换一根 USB 线 / 换一个 USB 端口

---

## 2026-05-21: 第3片验证 — ADC 零点偏移校准

### 问题12: 功率级未上电时 ADC 读到 35.6V（实际应为 0V）

**现象:** ADCINA0 读 `raw=729`，经量纲转换后 `v2_raw=35.6V`。功率级未上电，V2 应为 0V。

**排查过程:**
1. 确认 `raw=729` 折合 ADC 引脚 0.534V（729 × 3.0/4096）
2. 尝试软件触发零点校准 → `zero_offset=92`，与 ePWM 触发读数 (729) 差异大
3. **根因**: 软件触发 (SOC_SEQ1) 的采样条件与 ePWM SOCA 触发不同，两者读数不一致。改为 ISR 自行采集前 32 次 ePWM 触发样本取平均

**修复 — ISR 自动零点校准 (`adc.c`):**
- `adc_init()` 设 `zero_offset = 0xFFFF`（未校准哨兵值）
- ISR 前 32 次触发自动累加 ADC 原始值，取平均写入 `zero_offset`
- 校准期间 `raw` 输出 0，校准完成后正常减去偏移
- 32 × 100μs = 3.2ms 完成，对系统无影响

**校准结果:**
- `zero_offset = 859`（约 0.63V 等效引脚电压，源于 PWM 开关噪声耦合）
- 校准后功率级未上电：`raw=0`, `v2_raw=0.0V`, `v2_filtered=0.0V` ✓

**教训:** ADC 零点校准必须使用与实际运行相同的触发源（ePWM SOCA），不能改用软件触发取巧。

---

## 2026-05-21: 第3片验证 — 跳线帽与 ADC 通道

### 问题13: ADCINA0 始终读到噪声值 729，外部电压无法注入

**现象:** ADC 配置正确（CONV00=0→ADCINA0），ISR 正常触发（10kHz），但 `ADCRESULT0`
永远为 729 计数（≈0.534V），无论接不接外部电压、拧不拧电位器。

**排查过程:**
1. 学生电源接 ADCINA0 + GND → 读数不变，且仿真器频繁断连
2. 软件触发零点校准 → 与 ePWM 触发读数不一致，说明存在同步问题
3. 改用 ISR 自动采集（ePWM 触发）做零点校准 → 校准值 859~986，但外部电压仍不响应
4. 临时切通道到 ADCINB0（CONV00=8）→ 读数仍不变
5. **根因（查普中开发板文档发现）:** PZ-DSP28335-L 板上，电位器通过 **ADCA0_S**
   引脚输出，需要 **跳线帽短接** P8 端子的 ADCINA0 和 ADCA0_S 才能连通！

**修复:**
- P8 端子：短接 ADCINA0 ↔ ADCA0_S（跳线帽）
- 通道改回 ADCINA0（CONV00=0）
- 跳线后立即生效：`ADCRESULT0` 从固定的 11664 → 12624（随电位器位置变化）

**验证结果（zero_offset 临时清零后）:**
| 状态 | raw | ADC 引脚 | v2_raw |
|------|-----|---------|--------|
| 电位器低位 | 789 | 0.578V | 38.5V |
| 电位器拧动后 | 729 | 0.534V | 35.6V |

ADC 读数随电位器变化，采样 + IIR 滤波功能正常 ✓

**教训:**
1. 用开发板 ADC 前先查原理图，确认是否需要跳线帽
2. ADCINA0 是芯片引脚，ADCA0_S 是板级测试点，两者靠跳线连接
3. 接外部电压测试 ADC 时，学生电源的 GND 和仿真器 USB 地可能形成环路导致仿真器断连

---

## 2026-05-22: 第5片 PID 闭环 — 3 个运行时 Bug

### 问题14: systick 定时器不工作 — ConfigCpuTimer 通过空指针写寄存器

**现象:** 代码加载运行后 `g_pid.output = 0.0`，`control_step` 从未被调用，PID 完全未运行。

**排查过程:**
1. `evaluate` 读 `CpuTimer0Regs.TCR.all` → `0x8001`，解码：
   - TSS=1（定时器已停止）
   - TIE=0（中断未使能）
   - TIF=1（中断标志置位，说明定时器曾计数到但未产生中断）
2. `evaluate` 读 `CpuTimer0Regs.PRD.all` → `65535`（默认复位值）
   - ConfigCpuTimer 应该设置 PRD=999（1ms @ 150MHz），但 PRD 未变
3. `evaluate` 读 `CpuTimer0Regs.TPR.all` → `0`（默认值，应设为 149）
4. 根因：`systick_init()` 调用 `ConfigCpuTimer(&CpuTimer0, 150.0f, 1000.0f)`，该函数内部通过 `Timer->RegsAddr->PRD.all = ...` 写寄存器
5. 但 `InitCpuTimers()` 从未被调用 → `CpuTimer0.RegsAddr` 为 NULL
6. ConfigCpuTimer 所有寄存器写入都跳转到地址 0，实际未改变任何定时器寄存器

**关键区别:**
- `CpuTimer0Regs` 是宏，映射到物理地址 `0x00000C00`（在 `DSP2833x_CpuTimers.h` 中定义）
- `CpuTimer0` 是 `struct CPUTIMER_VARS` 实例，包含 `RegsAddr` 指针，需要 `InitCpuTimers()` 将 `CpuTimer0.RegsAddr = &CpuTimer0Regs` 初始化

**修复:** `systick_init()` 改为直接写 `CpuTimer0Regs.*` 寄存器：
```c
CpuTimer0Regs.TCR.bit.TSS = 1;    // stop
CpuTimer0Regs.TPR.all  = 149;      // prescaler
CpuTimer0Regs.PRD.all  = 999;      // period
CpuTimer0Regs.TCR.bit.TRB = 1;     // reload
CpuTimer0Regs.TCR.bit.TIE = 1;     // enable interrupt
CpuTimer0Regs.TCR.bit.TSS = 0;     // start
```
不依赖 `ConfigCpuTimer`/`CpuTimer0` 结构体指针。

**教训:** 使用 TI C2000 库函数前须确认其依赖的初始化函数是否已调用。ConfigCpuTimer 依赖 InitCpuTimers 初始化 RegsAddr 指针。对于简单配置，直接写寄存器更可靠。

### 问题15: 启动死锁 — k=1.0 导致 DPS 算法 D1=0

**现象:** 功率级未上电时 V2=0，系统完全卡死无法传输功率。

**根因分析:**
1. ADC ISR 在 V2<0.5V 时强制 `k = 1.0f`（防止除零）
2. DPS 公式 `D1 = (k-1)*A`（k≥1 分支），当 k=1.0 时 D1≡0
3. D1=0 → 零桥间移相 → 零功率传输 → V2 永远=0 → 死锁
4. 即使功率级上电，V2 从 0 启动也会遇到此死锁

**修复:** k 改为限幅到 [0.5, 3.0] 而非强制为 1.0：
```c
if (g_adc.v2_filtered > 0.5f) {
    float k_raw = V1_FIXED / (TRANSFORMER_RATIO * g_adc.v2_filtered);
    g_adc.k = (k_raw < 0.5f) ? 0.5f : ((k_raw > 3.0f) ? 3.0f : k_raw);
} else {
    g_adc.k = 3.0f;  // 启动：最大 k 限幅 → D1>0
}
```
修复后 V2=0 → k=3.0 → D1≈0.018（非零），启动死锁解除。

**数值验证:** p0=0.999, k=3.0 → D1=0.0183, D2=0.491

**教训:** 保护性代码（防止除零的 k=1.0 回退）可能在控制算法路径上引入意外行为。需要从系统层面验证对算法输入参数的约束，不能仅从单模块局部安全角度设计回退值。

### 问题16: 反积分饱和（anti-windup）失灵 — 空 `;` 分支被编译器误处理

**现象:** PID 输出饱和在 0.999（正常），但 `g_pid.integral = 3.28`（运行 2 秒）。反积分饱和逻辑应阻止积分累积，但实际在累积。

**排查过程:**
1. 验证输入：error=50, Kp=1.0 → p_term=50，out 应始终 > 0.999
2. 反积分饱和代码：
   ```c
   if (error > 0.0f) {
       ; // leave integral unchanged
   }
   ```
   理论上 integral 不应更新
3. integral=3.28 → Ki×error×dt=0.1×50×0.001=0.005/步 → ~656 步
4. 推测：TI Clang 编译器将空 `;` 分支优化掉，导致 `else` 分支的 `pid->integral = i_term` 被执行
5. 或者：第一个控制周期的 d_term=50000 导致 `out` 溢出/下溢到 `NaN`，破坏了后续比较逻辑

**修复:** 去掉空分支和所有条件嵌套，用最简结构：
```c
if (out > pid->out_max) {
    out = pid->out_max;
    // 不更新 integral（无条件）
} else if (out < pid->out_min) {
    out = pid->out_min;
    // 不更新 integral（无条件）
} else {
    pid->integral = i_term;  // 仅非饱和时更新
}
```
修复后 integral=0.0，反积分饱和正常。

**教训:** 不要在 C 代码中使用空 `;` 作为分支体。即使标准 C 允许，编译器可能在优化时产生意外行为。用显式的 else/不更新替代条件判断。

---

## 2026-05-22: 第7片验证 — 保护逻辑 3 个寄存器级 Bug

### 问题17: TZFRC 软件强制关断无效 — EALLOW 漏写

**现象:** protection_step 中 OVP 触发后，`g_prot_fault=1`、`g_fault_flag=1`、`g_ovp_count=3`，但 `EPwm1Regs.TZFLG.all=0`，PWM 输出未被关断。

**排查过程:**
1. 确认 protection_step 执行路径：OVP 检测→`tz_software_trip()`→`EPwm1Regs.TZFRC.bit.OST=1`
2. 读 `EPwm1Regs.TZFRC.all` → 0，写入未生效
3. 查 F28335 TRM：TZFRC 寄存器是 **EALLOW 保护**寄存器
4. 对比 `tz_clear()` 函数（同文件）：其中 TZCLR/TZFRC 都正确包裹了 EALLOW/EDIS
5. **根因**: `tz_software_trip()` 中 4 路 TZFRC 写入缺少 EALLOW/EDIS，编译器不报错，但 CPU 静默忽略写入

**修复:**
```c
static void tz_software_trip(void)
{
    EALLOW;  // ← 关键！
    EPwm1Regs.TZFRC.bit.OST = 1;
    EPwm2Regs.TZFRC.bit.OST = 1;
    EPwm3Regs.TZFRC.bit.OST = 1;
    EPwm4Regs.TZFRC.bit.OST = 1;
    EDIS;
}
```

**教训:** EALLOW 保护寄存器的写入被静默忽略——不报错、不警告、不下异常。写这类寄存器前必须确认 EALLOW 已执行。建议将 EALLOW 保护的寄存器写入封装在统一函数中，减少遗漏。

### 问题18: IDLE 状态下保护触发无法进入 STATE_FAULT

**现象:** 在 IDLE 状态（功率级未启动）设低 OVP 阈值触发保护：`g_prot_fault=1`、`g_fault_flag=1`，但 `g_system_state=0`（仍为 STATE_IDLE），`led5_off()`。

**排查过程:**
1. `protection_step` 正确执行：OVP 触发→`g_fault_flag=1`→return
2. 下一步 `state_machine_step` 执行：
   ```c
   if (g_fault_flag && g_system_state != STATE_IDLE) {
       g_system_state = STATE_FAULT;
   }
   ```
3. Phase 6 设计中 `g_fault_flag` 是 CCS 调试器手动设的 flag，仅当系统已在运行时（非 IDLE）才有意义转换到 FAULT
4. Phase 7 的 `protection_step` 可能在 IDLE 状态下触发故障（OVP/OCP 检测始终运行）
5. **根因**: 状态机 `g_fault_flag` 检查排除了 IDLE，但保护逻辑需要从任何状态进入 FAULT

**修复（两处联动）:**
- **`state_machine.c`**: 条件追加 `&& g_system_state != STATE_FAULT`，防止已处 FAULT 时每周期重复初始化
- **`protection.c`**: 所有故障触发路径追加 `g_system_state = STATE_FAULT`，不依赖 state_machine_step 做转换

```c
// protection.c — OVP 触发后：
g_prot_fault   = 1;
g_fault_flag   = 1;
g_system_state = STATE_FAULT;  // 直接设状态，不依赖 state_machine
tz_software_trip();
led3_on();
```

**教训:** 新增子系统（保护）接入已有框架（状态机）时，需要审查框架的进入条件是否覆盖新子系统的所有触发路径。IDLE 下的保护触发是合理的——V2 过压可能在任意时刻发生。

### 问题19: FAULT 状态下 LED5 快闪死锁 — led5_tick 每周期被清零

**现象:** FAULT 状态下 LED5 应 10Hz 快闪，但实际 LED5 熄灭不闪。

**排查过程:**
1. 确认 state_machine_step 进入 FAULT 后 `led5_mode = LED_FAST`
2. LED_FAST 分支：`if (++led5_tick >= FAST_PERIOD_MS)` → 50ms 翻转
3. 但 `g_fault_flag=1` 且 `g_system_state=STATE_FAULT` 时，state_machine_step 在 switch 前：
   ```c
   if (g_fault_flag && g_system_state != STATE_IDLE && g_system_state != STATE_FAULT) {
       // 修复前无 STATE_FAULT 检查
       led5_tick = 0;  // ← 每 1ms 被清零！
   }
   ```
4. 修复前条件仅为 `!= STATE_IDLE`，每周期进入 FAULT → `led5_tick=0` → 永远到不了 50ms → LED5 不翻转

**修复:** 条件追加 `&& g_system_state != STATE_FAULT`，确保仅首次进入 FAULT 时重置 ticks，后续周期不再干扰。

**教训:** 周期性重置计数器 + 计数器阈值触发的组合需要确保重置只发生一次。否则形成"每周期归零→永远达不到阈值"的死锁。

---


---

## 调试技巧总结

### 用 CCS evaluate 直接读外设寄存器
不需要在代码中声明外设变量。例如：
- `CpuTimer0Regs.TCR.all` → 查看/解码定时器控制寄存器
- `CpuTimer0Regs.PRD.all` → 检查周期值是否被正确设置
- `PieCtrlRegs.PIEIER1.all` → 确认 PIE 中断使能位

### 外设寄存器读值的诊断价值
在 CCS 暂停 CPU 后读外设寄存器是低侵入性诊断方式：
- 寄存器保留最后一次硬件状态，不受 CPU 暂停影响
- 可以快速判断"配置代码是否正确执行"（如 PRD 值是否为目标值）
- 结合数据手册寄存器位定义，可精确定位配置缺失

### 指针 vs 宏的区别
TI C2000 库中有两套外设访问方式：
- `CpuTimer0Regs` — 宏，直接映射到物理地址，只读/写寄存器
- `CpuTimer0` — 结构体变量，包含 `RegsAddr` 指针和其他元数据
- 使用结构体变量前需 `InitCpuTimers()` 初始化其指针字段
