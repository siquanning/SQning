# TMS320F28335 SST 前级整流控制上机测试步骤

> 适用工程：`E:\repos\resst`，目标板：TMS320F28335。本文按低压、空载、分阶段通电原则执行。未经前一级验证通过，不得进入下一级。所有涉及功率输入的接线、放电和测量必须由具备资质的人员完成，并使用隔离电源、限流装置、急停和合适量程的差分探头。

## 1. 测试目标与禁止事项

本测试依次验证：固件与安全初态、12路ADC零偏、Vdc测量比例、PLL锁相、GPIO22预充、GPIO23旁路、PWM释放、GPIO21停止和TZ故障保护。测试初期只使用低压、限流电源，禁止直接接入高压母线。

以下安全逻辑禁止为了调试而绕过：

- GPIO21停止后必须先执行 `PWM_BlockOutput()`，再断GPIO23，最后断GPIO22。
- TZ1/TZ2异常时禁止释放PWM。
- 预充、旁路等待和PLL等待期间PWM必须保持Block。
- 不得用ADC offset修正CT1/CT2/Gain比例误差。
- 未确认真实Vdc采样比例前，不得把预充门槛直接填写成20V、100V或800V。
- 不自动清除FAULT；出现FAULT后先断功率、放电、查明原因，再复位DSP。

## 2. 测试设备和接线准备

- 隔离、可调、带限流的低压三相电源或等效低压信号源。
- 示波器、隔离差分探头、电流探头、万用表。
- XDS仿真器和CCS；串口工具用于Modbus RTU。
- 确认GPIO21为保持型高有效启动输入；GPIO20为运行指示；GPIO22同时控制S1/S2/S3；GPIO23同时控制S4/S5/S6。
- 首次测试断开三相功率输入，确认6路直流电容已经放电。DSP控制板和采样板保持供电，以便ADC和CCS工作。

## 3. 编译、下载和上电安全初态

1. 使用CCS对 `Debug` 配置执行Full Build，确认0 errors。下载最新的 `E:\repos\resst\Debug\resst.out`，不要下载旧的 `Flash_Release\resst.out`。
2. GPIO21保持0后复位DSP，Resume运行。等待至少100ms，使50ms低电平消抖和初始化完成。
3. 检查GPIO：GPIO20=0、GPIO22=0、GPIO23=0；示波器确认全部ePWM门极输出被封锁。
4. CCS检查系统状态和诊断：

```text
Diagnostics_Get()->system_state        期望2（STANDBY）
Diagnostics_Get()->fault_code          期望0（FAULT_NONE）
Diagnostics_Get()->fast_isr_count      持续增加
g_adc_frame_count                      持续增加
g_start_seq_state                      期望0（IDLE）
g_grid_switch_cmd                      期望0
g_bypass_switch_cmd                    期望0
```

5. 查看 `Diagnostics_Get()->fast_isr.max_cycles`。140MHz下50µs等于7000 cycles；首次测试建议明显低于7000，并同时确认 `miss_1ms/miss_10ms/miss_100ms` 不持续增加。若接近或超过7000，停止后续功率测试，先处理ISR性能。

通过标准：无FAULT、ADC与20kHz ISR运行、三个开关输出均处于安全状态、PWM无脉冲。

## 4. 12路ADC零偏整定

### 4.1 确认零输入

断开功率输入并确认母线放电；Vac三相采样输入确实为0；Iac三相电流确实为0。DSP和采样电路继续供电。不能证明输入为0时，禁止校准该通道。

### 4.2 通过Modbus读取raw

如需Modbus，先把 `BOARD_DEBUG_JUSTFLOAT_ENABLE` 设为0，重新Full Build并下载。周期发送：

```text
01 03 00 00 00 0C 45 CF
```

回复中HR0～HR11依次为Vdc1～Vdc6、Vab/Vbc/Vca、Ia～Ic的16位raw，高字节在前。连续采集至少100帧，各通道独立求平均并四舍五入。不要用单帧值作为offset。不要用带电交流波形的均值当零偏。

### 4.3 CCS写入运行offset

```text
g_vdc1_offset_counts ... g_vdc6_offset_counts
g_vac_vab_offset_counts, g_vac_vbc_offset_counts, g_vac_vca_offset_counts
g_iac_ia_offset_counts, g_iac_ib_offset_counts, g_iac_ic_offset_counts
```

单位均为ADC count。Vac 零偏只校正 **线电压** 三路 ADC（Vab/Vbc/Vca）。JustFloat 用 lite mode 0 看 l0/l1/l2；CCS 看 `g_measurement.vline_v`。禁止对着重构相电压 `vac_v` 或 lite mode 2 调零偏。写入各通道raw平均值后观察：

```text
g_measurement.vdc_v[0..5]     接近0且不为负
g_measurement.vline_v[0..2]   围绕0小幅正负波动（Vab/Vbc/Vca）
g_measurement.vac_v[0..2]     线电压校零后应跟着绕0，不要用它反拧 offset
g_measurement.iac_a[0..2]     围绕0小幅正负波动
```

确认稳定后，把数值写回 `board_config.h` 中对应的 `*_OFFSET_COUNTS_DEFAULT`，重新编译、下载、复位并复查。Offset只校正零点；若输入翻倍而软件结果不近似翻倍，应检查CT1/CT2/Gain，不得继续调offset。

## 5. Vdc测量比例确认与预充参数初设

当前Vdc标定为CT1=1000V:2V、CT2=1:1、实测后级Gain=0.5，因此软件值约为：

```text
Vdc = max(raw-offset,0) × 3/4095 × 1000/2
    ≈ corrected_raw × 0.73260 V
```

这时 `g_measurement.vdc_v[]` 表示按已确认CT1比例换算的一次侧电压，理论软件满量程约1500V。使用安全隔离的一次侧低压点逐路比对raw、软件Vdc和万用表值，检查零点、线性及通道一致性；任何ADC引脚仍不得超过0～3V范围。

真实硬件比例确认后，才可修改：

```text
BOARD_VDC_CT1_PRI_V / BOARD_VDC_CT1_SEC_V
BOARD_VDC_CT2_PRI_V / BOARD_VDC_CT2_SEC_V
BOARD_VDC_ANALOG_GAIN
BOARD_ADC_VREF_V
```

现场先根据软件实际显示值设置运行参数：

```text
g_precharge_done_v
g_precharge_timeout_ms
g_bypass_delay_ms
```

固件永久默认门槛为400V，当前实测比例下相当于每路 `raw-offset` 约546 counts；含义是六路最低一路也必须达到400V，并非六路合计400V。进行低压实验前，必须根据低压电源和六路稳定显示值，在CCS Expressions将 `g_precharge_done_v` 临时调低并保留安全余量；DSP复位后会恢复400V默认值。

## 6. PLL独立测试

PLL测试期间GPIO21保持0、PWM Block、GPIO22/23保持0。给Vac采样端输入低压、限流、相序正确的三相50Hz信号。逐步升高到满足现有PLL电压门控，严禁为了“看见锁定”直接绕过PLL判据。

CCS观察：

```text
g_measurement.vline_v[0..2]
g_measurement.vac_v[0..2]
g_pll.vmag
g_pll.freq
g_pll.vd
g_pll.vq
g_pll.theta
g_pll_switch_req
g_switch_alpha
g_switch_phase_err_deg
```

通过标准：三相波形幅值和相序正确；`g_pll.freq`稳定在50Hz附近；`|vq|`明显小于`vmag`且`vd`为正；连续满足现有判据约200ms后 `g_pll_switch_req=1`；随后约200ms内 `g_switch_alpha`平滑升到1附近，无突跳和FAULT。撤去或降低Vac后，应按现有失锁/holdover逻辑变化，不得出现非法PWM释放。

当前 `BOARD_PLL_LOCK_VMAG_MIN_V=10.0V`，表示 Clarke 变换后的**相电压** αβ 矢量峰值门槛。硬件采的是线电压，软件先重构相电压再进 PLL。对于 220Vrms 相电压，正常幅值约 311V。相电压峰值超过 10V（约 7.1Vrms 相电压 / 12.2Vrms 线电压）才能越过门槛。`g_pll.freq≈50Hz` 不能单独证明锁定，因为门槛以下 PLL 会执行 50Hz holdover。

若 PLL 不锁，依次检查：线电压 Vab/Vbc/Vca 的 offset（`g_vac_vab/vbc/vca_offset_counts`）、三相通道映射、相序、`g_pll.vmag` 是否超过 10V、频率、vd/vq 和 TZ 状态。不要先修改 PI、theta、90° 补偿或 alpha 算法。

## 7. 不控整流软启动与预充测试

### 7.1 PRECHARGE

1. 保持低压、限流，确认PLL测试已经通过；GPIO21先保持0至少50ms。
2. CCS观察：

```text
g_precharge_done_v
g_precharge_timeout_ms
g_precharge_vdc_min
g_start_seq_state
g_start_seq_timer_ms
g_start_seq_fail
g_grid_switch_cmd
g_bypass_switch_cmd
g_measurement.vdc_v[0..5]
```

3. 将GPIO21置1并保持。期望同拍开始启动序列：`g_start_seq_state=1`，GPIO22=1，GPIO23=0，GPIO20=0，PWM仍Block。
4. 示波器/万用表观察6路直流电容经预充电阻上升。所有通道都应合理上升；`g_precharge_vdc_min`等于六路最低值。
5. 当最低一路达到 `g_precharge_done_v` 时，GPIO23应变1并进入 `g_start_seq_state=2`。若10s内未达到，`g_start_seq_fail=1`、GPIO23=0、GPIO22=0且禁止RUN。超时后必须先将GPIO21置0，再排查采样、接线、门槛和预充回路。

### 7.2 BYPASS_WAIT与RUN

GPIO23闭合后，PWM仍必须Block至少 `g_bypass_delay_ms`。延时结束后只有同时满足PLL就绪、`g_switch_alpha >= g_pll_ready_alpha_min`、TZ1/TZ2正常和无FAULT，才能RequestRun并释放PWM。

通过标准：GPIO22/23保持1；延时期间门极无PWM；条件全部满足后GPIO20=1、系统状态为RUN、PWM波形出现；六路Vdc无明显异常跌落或严重不一致。首次释放PWM应使用最低可行母线电压、严格限流并监视电流，发现异常立即将GPIO21置0或使用硬件急停。

## 8. STOP与TZ安全回归

分别在PRECHARGE、BYPASS_WAIT和RUN阶段将GPIO21置0并保持超过50ms。用示波器和GPIO观察验证固定顺序：

```text
第一：PWM立即Block，主动开关停止
第二：GPIO23=0，退出预充电阻旁路
第三：GPIO22=0，切断三相输入
随后：启动状态清零、回STANDBY、GPIO20=0
```

三个阶段均必须通过。RUN阶段再进行受控TZ测试：使用安全的故障注入方式使TZ1或TZ2进入有效电平，确认GPIO30立即拉低、PWM被硬件OST封锁、系统进入FAULT、GPIO20熄灭且不会自动重启。故障解除后仍不得自动RUN；当前版本无生产清故障入口，应断功率、放电、复位后恢复。

## 9. 测试记录与最终放行

每次测试至少记录：日期、固件 `.out` 时间和CRC、供电电压/限流值、12路offset、CT1/CT2/Gain、预充三个参数、PLL关键量、Vdc1～Vdc6、ISR最大cycles、状态/故障码、GPIO时序和示波器截图。

| 阶段 | 关键结果 | 通过/失败 | 备注 |
|---|---|---|---|
| Debug全量构建与下载 | 0 errors，模块链接完整 |  |  |
| 上电安全初态 | 22/23/20=0，PWM Block |  |  |
| 12路offset | 零输入换算接近0 |  |  |
| Vdc比例 | 多点线性及比例正确 |  |  |
| PLL | 50Hz、vq小、req=1、alpha≈1 |  |  |
| PRECHARGE | 六路上升，最低值达门槛 |  |  |
| BYPASS_WAIT | GPIO23=1且延时期间PWM Block |  |  |
| RUN | PLL/TZ/FAULT许可后才释放PWM |  |  |
| STOP三阶段 | PWM→GPIO23→GPIO22顺序正确 |  |  |
| TZ故障 | 硬件封锁、FAULT锁存、不自启 |  |  |

只有上述项目全部通过，且低压重复启动/停止至少5次无异常，才允许制定下一阶段升压计划。升压必须逐级进行，每一级重新确认Vdc比例、预充时间、电流峰值、旁路瞬态、PLL和TZ保护；不得从低压结果直接跳到高压满功率测试。
