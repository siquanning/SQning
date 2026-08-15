# TMS320F28335 SST/CHB 前级整流 DSP 工程设计与核查报告

**工程路径：** `E:\repos\resst`  
**报告版本：** V1.0  
**报告日期：** 2026年8月14日  
**适用阶段：** 单相逐相、低压、限流样机调试

---

## 摘要

本工程以 TMS320F28335 为控制核心，面向 SST/CHB 前级整流功率级，实现三相电压、三相电流和六路直流侧电压采样，SRF-PLL 同步，直流电压外环与交流电流内环双闭环，六模块 ePWM 钳位式单极性调制，以及基于 Trip-Zone、GPIO30 总门极使能和状态机的安全启停。

当前软件已经一次性完成 A/B/C 三相的软件映射，但保持“单相逐相测试”运行方式。现场通过 `g_ctrl_test_phase=1/2/3` 选择 A、B 或 C 相；测试相只在进入 RUN 前锁存，RUN 期间修改该变量不会造成热切换。任何一次 RUN 只释放目标相的两个 ePWM 模块，其余四个 ePWM 始终保持 OST 硬件封锁。

经代码核查、16 组主机测试以及 Debug、Release、Industrial_RAM、Flash_Release 四配置真实 C28x 编译链接验证，工程主流程完整，未发现阻止低压逐相运行的代码级错误。工程当前适合进入“无高压确认—低压限流—A/B/C 逐相验证”阶段，尚不应直接视为已完成额定电压、额定功率验证。

## 1. 工程目标与运行边界

### 1.1 已实现能力

- 三相电网电压 Va/Vb/Vc 同步采样并运行 SRF-PLL；
- 六路直流电压 Vdc1～Vdc6 采样；
- 三相交流电流 Ia/Ib/Ic 采样；
- 单套公共的直流电压外环和交流电流内环；
- A/B/C 测试相的软件选择与启动前锁存；
- 目标相 PWM 释放、非目标相 OST 封锁；
- 三相完整 Trip-Zone 硬件及软件故障链；
- PLL 及当前闭环相的 JustFloat 调试输出；
- 预充、旁路、PLL 就绪、PWM 释放、STOP 与 FAULT 安全流程。

### 1.2 当前运行边界

本阶段不实现三相同时闭环运行。任何一次 RUN 只允许一相工作：

| `g_ctrl_test_phase` | 测试对象 | 工作 PWM | 保持 OST 的 PWM |
|---:|---|---|---|
| 1 | A 相双闭环 | ePWM1、ePWM2 | ePWM3～ePWM6 |
| 2 | B 相双闭环 | ePWM3、ePWM4 | ePWM1、ePWM2、ePWM5、ePWM6 |
| 3 | C 相双闭环 | ePWM5、ePWM6 | ePWM1～ePWM4 |
| 其他 | 非法选择 | 无 | ePWM1～ePWM6 |

换相必须遵守“STOP → 修改相别 → 重新 RUN”。RUN 期间即使 CCS 修改 `g_ctrl_test_phase`，本次运行的 active phase 也不会改变。

## 2. 系统硬件与软件架构

### 2.1 主要硬件和时序参数

| 项目 | 当前配置 | 说明 |
|---|---:|---|
| DSP | TMS320F28335 | C28x + FPU32 |
| 外部晶振 | 20 MHz | 板载 Y1 |
| SYSCLK | 140 MHz | PLL ×7 |
| PWM 频率 | 20 kHz | 周期 50 μs |
| PWM 计数模式 | 上下计数 | TBPRD=3500 |
| 死区 | 1.0 μs | DBRED=DBFED=140 |
| ePWM 数量 | 6 | 每相两个半桥模块 |
| ADC 通道 | 12 | 6×Vdc、3×Vac、3×Iac |
| 低速调度基准 | 100 μs | CPU Timer0 |
| 电压外环 | 1 kHz | 1 ms 调度 |
| 电流内环/PLL | 20 kHz | ePWM1 周期 ISR |

### 2.2 软件分层

1. BSP 层负责板级时钟、ADC、ePWM、GPIO 和安全输出封锁。
2. Driver 层直接访问 F28335 外设寄存器。
3. Control 层实现 PLL、开环参考、双闭环 PI 和调制计算。
4. Service 层实现测量换算、JustFloat、通信及指示服务。
5. App 层实现 ISR、调度、启停监督和状态机。

核心信号链如下：

```text
ADC采样
  → 物理量换算
  → SRF-PLL
  → 当前测试相选择
  → Vdc外环（1kHz）
  → Iac内环（20kHz）
  → 调制量m
  → 当前相两个ePWM
  → CPLD门极逻辑
```

## 3. ADC 映射与物理量换算

### 3.1 ADC 通道映射

| 转换序号 | ADC 输入 | 软件量 |
|---:|---|---|
| CONV00 | ADCINB6 | Vdc1 |
| CONV01 | ADCINB7 | Vdc2 |
| CONV02 | ADCINB4 | Vdc3 |
| CONV03 | ADCINB5 | Vdc4 |
| CONV04 | ADCINB2 | Vdc5 |
| CONV05 | ADCINB3 | Vdc6 |
| CONV06 | ADCINB1 | Va |
| CONV07 | ADCINB0 | Vb |
| CONV08 | ADCINA0 | Vc |
| CONV09 | ADCINA7 | Ia |
| CONV10 | ADCINA6 | Ib |
| CONV11 | ADCINA5 | Ic |

ADC 由 ePWM1 在 CTR=ZERO 时产生 SOCA 触发。12 路级联转换完成后，ADC ISR 更新 raw 缓存；20 kHz 控制 ISR 使用最近一次完整 ADC 帧，因此存在固定的一拍采样流水延迟。

### 3.2 Vdc 正确换算关系

当前实际换算公式为：

```text
Vdc[V] = max(raw-offset, 0)
         × (3.0/4095)
         × (1.0/1.0)
         × (1000.0/2.0)
         ÷ 0.5

       = corrected_raw × 0.7326007 V/count
```

实测锚点：

```text
corrected_raw = 546
Vdc = 546 × 0.7326007 ≈ 400 V
```

该关系同时用于 1 kHz 测量快照和 20 kHz 闭环实时换算，不存在两套 Vdc 比例。

### 3.3 Vac 与 Iac 换算

按当前硬件参数推导：

```text
Vac[V] = polarity × (raw-offset) × 2.63210 V/count
Iac[A] = polarity × (raw-offset) × 0.058608 A/count
```

Vac/Iac 默认偏置均为 2048 counts，默认极性均为 +1。现场必须分别验证三相偏置、比例、相序和电流方向，不能仅通过 Vdc 标定推断交流测量正确。

当前 0.5 A 峰值电流限制仅对应约 8.53 个 ADC count，分辨率较粗。低电流测试时应重点观察零点噪声和电流反馈抖动。

## 4. SRF-PLL 与相位接管

PLL 在 20 kHz ISR 中持续运行，使用三相瞬时电压完成 Clarke/Park 变换，以 vq 为误差信号调整频率和相位。

| 参数 | 当前值 |
|---|---:|
| 额定频率 | 50 Hz |
| 比例增益 Kp | 120 |
| 积分增益 Ki | 2000 |
| 频率限制 | 45～55 Hz |
| 锁定频率范围 | 49.5～50.5 Hz |
| `|vq|/vmag` 门槛 | 3% |
| `vd/vmag` 门槛 | 0.9 |
| 最低有效幅值 | 50 V 峰值 |
| 锁定确认时间 | 200 ms |
| 失锁确认时间 | 100 ms |
| LUT→PLL 淡入时间 | 200 ms |

PLL 锁定前保持开环 LUT 相位运行；锁定确认后，软件通过 alpha 从 0 到 1 淡入 PLL 相位，避免瞬间切换。进入 RUN 还要求 `g_switch_alpha≥0.999`。

PLL 失锁确认后的处理顺序为：先执行 `PWM_BlockOutput()` 完成硬件封锁，再清除 active phase 并进入 `FAULT_HW_PLL_LOCK_LOST`。因此不会把 `m=0` 误认为功率管关闭。

## 5. 双闭环控制算法

### 5.1 三相公共映射

| 测试相 | Vac | Iac | 直流侧 | 相角 | PWM |
|---|---|---|---|---|---|
| A | Va | Ia | Vdc1、Vdc2 | θ | ePWM1、ePWM2 |
| B | Vb | Ib | Vdc3、Vdc4 | θ−2π/3 | ePWM3、ePWM4 |
| C | Vc | Ic | Vdc5、Vdc6 | θ+2π/3 | ePWM5、ePWM6 |

所有相共用同一套 PI 和状态变量；切换测试相不会复制或切换三套控制器。

### 5.2 直流电压外环

当前相直流电压定义为：

```text
Vdc_avg = (Vdc_upper + Vdc_lower) / 2
```

进入闭环的第一拍执行：

```text
vdc_ref_ramp = 当前Vdc_avg
vdc_integral = 0
i_integral   = 0
Iamp         = 0
```

随后参考值按 10 V/s 斜坡上升，最高到 450 V。电压 PI 输出为电流幅值 Iamp，并限制在 0～0.5 A。PI 采用条件积分抗饱和。

### 5.3 交流电流内环

```text
Iref    = power_sign × Iamp × cos(theta_phase)
i_error = Iref - Iac
vctrl   = Kp_i × i_error + integral_i
m       = (Vac - vctrl + Rgrid × Iac) / (Vdc1 + Vdc2)
```

调制量限制为 `-0.20≤m≤0.20`，电流积分器采用条件积分抗饱和。当前 `Rgrid=0`，`power_sign=+1`，三相共用同一套参数。

### 5.4 当前双闭环参数

| 参数 | 当前值 | 含义 |
|---|---:|---|
| VDC_TARGET | 450 V | 当前相平均直流电压目标 |
| VDC_RAMP_RATE | 10 V/s | 参考斜坡 |
| I_LIMIT | 0.5 A | 电流峰值限制 |
| M_LIMIT | 0.20 | 调制量绝对值限制 |
| Kp_v | 0.02 | 电压环比例增益 |
| Ki_v | 2.0 | 电压环积分增益 |
| Kp_i | 6.0 | 电流环比例增益 |
| Ki_i | 1200.0 | 电流环积分增益 |
| Rgrid | 0 Ω | 网侧等效电阻前馈项 |
| power_sign | +1 | 电流方向选择 |

`M_LIMIT=0.20` 属于保守调试限制。在 220 Vrms 相电压、Vdc1+Vdc2 约 800 V 时，仅电网电压前馈就约需 `m=311/800≈0.389`，因此额定电网下会发生调制饱和。该参数适合低压首次验证，不适合作为额定运行最终值。

## 6. PWM 调制与 Trip-Zone 保护

### 6.1 钳位式单极性调制

```text
m > 0：左桥臂钳位高，右桥臂斩波
m < 0：右桥臂钳位高，左桥臂斩波
m = 0：两个桥臂均钳位高，桥输出电压为零
```

`m=0` 只是零电压状态，不表示全部功率管关闭。真正关闭依赖 GPIO30 和 Trip-Zone OST。

### 6.2 TZ 配置

ePWM1～ePWM6 使用完全一致的 TZ 配置：

```text
TZSEL = 0x0300 = OSHT1 + OSHT2
TZ1   = GPIO12，低有效
TZ2   = GPIO13，低有效
TZA   = FORCE_LOW
TZB   = FORCE_LOW
```

同相两个 ePWM 共用相同 TZ1/TZ2 源。软件选取 ePWM1、ePWM3、ePWM5 分别作为 A/B/C 相的代表 TZ 中断模块，并将三个 PIE 向量绑定到统一 `App_EpwmTzIsr()`。

### 6.3 TZ 故障处理链

任意当前测试相发生硬件 TZ 后：

1. GPIO30 立即拉低，CPLD 封锁全部门极；
2. 关闭六路 ePWM 的 OST 中断；
3. 六路 ePWM 全部强制 OST；
4. 清除当前 active phase；
5. 锁存 `FAULT_HW_TZ_TRIP`；
6. 后台 FAULT 流程先断开 GPIO23，再断开 GPIO22；
7. 禁止自动重新启动。

该设计同时保证 PWM 硬件已被拉低和软件状态已经进入 FAULT，不存在 B/C 相跳闸后软件仍显示 RUN 的缺口。

## 7. 启动、停止与故障流程

### 7.1 启动流程

上电后系统保持 STANDBY，GPIO30、GPIO22、GPIO23 为低，六路 ePWM 均处于 OST。GPIO21 为保持型自锁按钮，必须先观察到稳定低电平解除 restart inhibit，再通过稳定高电平提出启动请求。

1. 保持 PWM 全局封锁；
2. GPIO23 保持断开；
3. GPIO22 闭合，进入不控整流预充；
4. 六路 Vdc 最低值达到 400 V；
5. GPIO23 闭合，旁路预充电阻；
6. 等待至少 500 ms；
7. 确认 PLL 锁定、相位淡入完成、TZ1/TZ2 实时输入正常；
8. 锁存 `g_ctrl_test_phase`；
9. 状态机进入 RUN；
10. 仅清除目标相两路 OST；
11. 最后将 GPIO30 拉高。

预充最长允许时间为 10 s。超时后 PWM 保持封锁，GPIO23 和 GPIO22 断开，且要求 GPIO21 重新回到低电平后才能再次尝试。

### 7.2 正常 STOP

1. GPIO30 拉低；
2. 关闭全部 OST 中断；
3. 六路 ePWM 强制 OST；
4. GPIO23 断开；
5. GPIO22 断开；
6. 清除 active phase；
7. 状态回到 STANDBY。

软件 STOP 不会被误识别为硬件 TZ，因为强制 OST 前已经关闭全部 OST 中断。

### 7.3 FAULT

硬件 TZ、PLL 确认失锁、调度异常和通信异常等均可进入 FAULT。`first_fault` 只锁存第一次故障，FAULT 状态禁止自动重启。当前样机阶段建议故障后复位或断电恢复，不在高压条件下尝试在线清故障。

## 8. JustFloat 与 CCS 调试

现场主要使用：

```text
g_ctrl_test_phase = 1 / 2 / 3
g_debug_view       = 1 / 2
```

`g_debug_view=1` 为 PLL 视图；`g_debug_view=2` 为当前测试相双闭环视图：

| 通道 | 内容 |
|---:|---|
| CH0 | 当前相 Vac |
| CH1 | 当前相 Iac |
| CH2 | 当前相 Iref |
| CH3 | 当前相 Vdc_avg |
| CH4 | Vdc_ref_ramp |
| CH5 | Iamp |
| CH6 | m |
| CH7 | 当前相 theta_phase |

切换 A/B/C 时不需要修改 JustFloat 通道定义。

## 9. 验证结果

### 9.1 主机测试

全部 16 组主机测试通过。与本阶段直接相关的覆盖包括：

- A/B/C 相映射正确；
- 非法 phase 拒绝；
- RUN 期间修改请求相不会热切换；
- 每种选择下仅目标相允许清 OST；
- 六路 ePWM 的 TZSEL 均为 0x0300；
- A/B/C 代表 TZ 进入统一 FAULT 入口；
- Vdc raw=546 对应约 400 V；
- 双闭环接管、斜坡与抗饱和；
- JustFloat 闭环通道跟随当前相。

### 9.2 四配置构建

| 构建配置 | 结果 |
|---|---|
| Debug | 编译、链接通过 |
| Release | 编译、链接通过 |
| Industrial_RAM | 编译、链接通过 |
| Flash_Release | 编译、链接通过 |

Industrial_RAM 主要内存使用情况：

| 区域 | 已用 | 总量 | 约占比 |
|---|---:|---:|---:|
| RAML03 | 0x2C9D | 0x4000 | 69.7% |
| RAML4 | 0x08F6 | 0x1000 | 56.0% |
| RAML5 | 0x0252 | 0x1000 | 14.5% |

当前无链接溢出，尚有可用程序和数据空间。

## 10. 当前限制与风险判断

### 10.1 不阻止低压运行的限制

- `M_LIMIT=0.20` 在额定电网下会饱和，必须从低压开始；
- 0.5 A 仅约 8.53 个 ADC count，电流反馈分辨率较粗；
- 当前控制路径未增加数字滤波，现场需观察噪声；
- 20 kHz 控制使用上一完整 ADC 帧，存在固定一拍延迟；
- 工程中部分历史注释仍保留旧的 ADC 理论检查数值，应以实际公式和实测标定为准。

### 10.2 提高功率前必须确认

1. 实测 TZ1/TZ2 能在 A/B/C 三种测试选择下立即拉低全部 PWM。
2. 确认 GPIO30 在 STOP 和 FAULT 时始终为低。
3. 完成 Vdc、Vac、Iac 的比例、偏置、极性和相序校验。
4. 读取 `g_diagnostics.fast_isr.max_cycles`，确认明显小于 7000 cycles。
5. 当前实际闭环 ISR 没有接入 `Control_FastStep()` 中的 ADC 卡死/越界软件保护；提高电压前应以已验证的硬件 TZ 保护作为必要前提。
6. 确认低压下 `g_power_sign=+1` 对应整流升压方向；方向不正确时先封锁 PWM 再修改。

## 11. 推荐上机流程

### 11.1 无高压检查

1. 烧录 Debug 或 Industrial_RAM 配置。
2. 保持功率侧断电，检查 GPIO30、GPIO22、GPIO23 初始状态。
3. 查看六路 `TZFLG.OST` 和 `TZEINT.OST`。
4. 检查 ePWM1～ePWM6 的 `TZSEL=0x0300`、`TZCTL` 强制低动作。
5. 分别拉低 TZ1 和 TZ2，确认统一 FAULT 链。
6. 检查 20 kHz ISR 计数和 ADC 帧计数持续增长。

### 11.2 低压逐相测试

1. 设置 `g_debug_view=2`。
2. 设置 `g_ctrl_test_phase=1`，低压、限流测试 A 相。
3. 确认只有 ePWM1/2 解除 OST，观察 Vac、Iac、Iref、Vdc、m 和 theta。
4. STOP 并确认六路重新进入 OST。
5. 设置 `g_ctrl_test_phase=2`，重新 RUN 测试 B 相。
6. 再次 STOP，设置 `g_ctrl_test_phase=3` 测试 C 相。
7. 禁止在 RUN 期间直接修改 1→2→3 尝试换相。

### 11.3 升压原则

只有在三相分别完成方向、TZ、采样、PLL、PWM 和闭环波形确认后，才逐步提高输入电压和 `M_LIMIT`。每次只改变一个参数，保留示波器和 JustFloat 记录，出现异常时优先 STOP 或触发硬件保护。

## 12. 结论

当前工程已经形成一条完整、可编译、可测试的单相双闭环运行链。A/B/C 三相的软件映射、PWM 选择、OST 封锁、TZ 软件故障入口和 JustFloat 视图均已一次配置完整，同时通过启动前锁存机制禁止 RUN 期间热切换。

综合判断，工程可以进入低压限流逐相样机测试。当前最关键的现场验收项不是继续重构代码，而是确认三类采样标定、A/B/C 实际门极映射、硬件 TZ 动作、`g_power_sign` 方向和 20 kHz ISR 时间余量。在完成这些确认前，不建议直接进行额定电压或额定功率运行。

## 附录 A：主要代码位置

| 功能 | 文件 |
|---|---|
| 板级参数与测量链 | `firmware/bsp/board_config.h` |
| ePWM 初始化与 TZ 寄存器 | `firmware/drivers/drv_epwm.c` |
| PWM 全局封锁/逐相释放 | `firmware/bsp/board.c` |
| PIE 中断绑定 | `firmware/drivers/drv_interrupt.c` |
| 20 kHz 控制与统一 TZ ISR | `firmware/app/isr.c` |
| 启动、预充、STOP/FAULT | `firmware/app/run_supervisor.c` |
| 测量换算 | `firmware/services/measurement.c` |
| SRF-PLL | `firmware/control/control_pll.c` |
| 双闭环 PI | `firmware/control/control_closedloop.c` |
| JustFloat | `firmware/services/justfloat.c` |
| 主机测试入口 | `tests/host/_all.bat` |
| 四配置构建入口 | `tools/build_all.ps1` |

## 附录 B：现场建议观察变量

| 分类 | 变量 |
|---|---|
| 测试选择 | `g_ctrl_test_phase`、active phase |
| 系统状态 | state、first_fault、restart inhibit |
| 启动序列 | `g_start_seq_state`、`g_start_seq_timer_ms` |
| 电源路径 | `g_grid_switch_cmd`、`g_bypass_switch_cmd` |
| PLL | theta、freq、vd、vq、vmag、alpha |
| 闭环 | `g_vdc_a`、`g_vdc_ref_ramp`、`g_iamp_a`、`g_ia_ref`、`g_m_a` |
| ADC | `g_vdc_raw[]`、`g_vac_raw[]`、`g_iac_raw[]`、`g_adc_frame_count` |
| 时间 | `g_diagnostics.fast_isr.max_cycles` |

