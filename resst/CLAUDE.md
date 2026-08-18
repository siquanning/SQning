<!-- DO NOT EDIT - This part is automatically generated. -->

# Claude Code Guidelines

## MANDATORY Pre-Task Steps (DO NOT SKIP)

**CRITICAL - NO EXCEPTIONS**: Before ANY CCS/Texas Instruments-related task (even simple ones), you MUST complete these steps IN ORDER. Do NOT call any ccs-project, ccs-debug, ccs-sysconfig, or ccs-serial MCP tools until both steps are complete.

1. Read `.claude/ccs.settings.md` to get the CCS installation directory
2. Read `{installation_directory}/ccs/theia/resources/ai/CCS.md` using the installation directory from step 1. This file includes information on how to interact with CCS as well as device-specific information (UART backchannel pins, LED setup, transmit best practices, etc.).
3. ONLY THEN proceed with CCS MCP tool calls or any other task work

Do NOT parallelize these steps with task work. Do NOT skip step 2 regardless of task complexity.


<!-- DO NOT EDIT - This part is automatically generated. -->

<!-- User instructions should be added below this line -->

> Created by Siquanning

## 工程路径（严格区分）

| 角色 | Windows | WSL |
|---|---|---|
| CCS Workspace（仅元数据，不含源码） | `D:\repos\DSP工作区` | `/mnt/d/repos/DSP工作区` |
| CCS Project（唯一源码工程） | `D:\repos\resst` | `/mnt/d/repos/resst` |

读码/编辑走 WSL `/mnt/d/repos/resst`；真实构建走 Windows `D:\repos\resst`。不要把源码搬进 Workspace。

## 构建（cl2000 是唯一编译判据，clangd 只做导航）

- 20MHz 正式板（100MHz）：`powershell -File tools\build_all.ps1`（四配置：Debug/Release/Industrial_RAM/Flash_Release）
- 30MHz 开发板（150MHz）：`powershell -File tools\build_dev30_debug.ps1`（产物 `Debug\resst_dev30_150mhz_debug.out`）
- **新增源文件后需让 CCS 重新生成 Debug/makefile**（否则 gmake 构建链接失败）
- 修改代码后至少一次真实 Debug 构建；任务完成前完整 4-config 构建；主机测试 `tests\host\_all.bat`（18 项）

## 关键宏状态（board_config.h）

- `BOARD_PLL_RELAY_TEST_ONLY=0`：双闭环启用（当前）
- `BOARD_LOW_VOLTAGE_DIRECT_TEST=1`：低压直测（GPIO42/44 同开，跳过预充）
- `BOARD_CLOCK_BRINGUP_ONLY`：仅与 DEV_30MHZ 同用，功率永久封锁
- 禁止修改：`PWM_BlockOutput/PWM_ReleaseOutput`、GPIO30/TZ 安全链、GPIO42/44、GPIO21、状态机、PLL 算法、m 限幅

## 改代码工作流（用户要求，必须遵守）

**先列清单、等批准、再动手**：任何改动工程代码前，一次性列出全部待改文件与每处具体改动（改前→改后 + 目的 + 对安全链影响 + 验证计划），等用户审阅并授予文件写权限；不要一条一条追问。纯读码/审查/构建/测试不需此流程。

## 调试体系

- JustFloat：SCI-C 576000、1kHz；固定 7 通道/32B，`g_jf_lite_mode=0` 发线电压+电流、`=1` 发 Vdc1~6、`=2` 发相电压+PLL跟随波、`=3` 发 VdcAvg+Iac/vd_ctrl/iamp/Id/Iq/m（见 `串口JustFloat数据说明.md`）
- 串口参数协议：SET（group 0xFF/0xFE）+ GET（group 0xFC → 响应 0xFD），白名单 + 上下限
- 实机调试：TI DSS（`E:\ti\ccs2051\ccs\ccs_base\scripting\bin\dss.bat`）+ XDS100v3，脚本在 `D:\repos\DSP工作区\_dss_bringup\`
- 功率级 WRITE（释放 PWM/GPIO30/烧 Flash 等）必须先报告经授权
- 无实机证据的结论一律标「需要硬件验证」

## 安全红线

- 30MHz 板烧 DEV_30MHZ 固件，20MHz 板烧 TARGET_20MHZ 固件，禁止混烧
- 上电默认安全：GPIO30/42/44=0、PWM 全 TZ 封锁、`restart_inhibit=1`
- 不自动 git 写操作、不烧写、不自动启动调试会话

## 上电调试 checklist（低压逐相）

1. 烧对固件：30MHz 板 DEV_30MHZ（`resst_dev30_150mhz_debug.out`）、20MHz 板 TARGET_20MHZ；Watch 确认 `g_board_sysclk_hz` 与板子匹配
2. 上电 Watch：GPIO30/42/44=0、PWM 全 TZ 封锁、`g_diagnostics.fast_isr.max_cycles`<7000
3. TZ1/TZ2（GPIO12/13）正常电平；手动拉低一次验证硬件封锁与 `FAULT_HW_TZ_TRIP`
4. 低压 20~30Vrms + 限流：lite mode 0 看三路线电压；mode 2 看相电压与 PLL 跟随波重合；CCS 确认 `g_pll.freq≈50`、`vq≈0`、`g_pll_switch_req=1`
5. 标定采样（线电压 Vab/Vbc/Vca 零偏、Iac 零偏、Vdc 锚点）；lite mode 0 看线电压校零，lite mode 1 看六路母线；禁止用 mode 2 调零偏
6. 首次 RUN（只放目标相）：CCS 验收 dq 方向（**Id>0、Iq≈0**）；看 `g_phase_ctrl[X].i_alpha/i_beta`（Iα/Iβ 差 90°）
7. CCS 看母线稳压与 Iamp、`g_phase_ctrl[X].m` 是否饱和；异常先看 `first_fault`
8. 升压原则：一次只改一个参数；每级确认采样不削顶、m 不持续饱和、六路母线平衡
9. 预充流程：`g_run_sup.seq_state` 0→1(PRECHARGE)→2(BYPASS_WAIT)→RUN；**高压前把 `g_precharge_done_v` 门槛提到母线目标附近**（默认 5.0V 形同虚设）
10. 首次 RUN / 释放 PWM / 烧 Flash 属功率级 WRITE，先报告经授权

## 串口命令速查（SCI-C 576000，7 字节帧 `[group][FF][cmd][float LE 4B]`）

| 目的 | 帧 |
|---|---|
| 切通道组（0=线电压+Iac，1=Vdc，2=相电压+PLL跟随，3=VdcAvg+Iac/vd_ctrl/iamp） | `FE FF 04 00 00 00 0X` |
| JustFloat 开关（0/1） | `FE FF 00 00 00 00 0X` |
| 观测相 0~3（0=跟随测试相） | `FE FF 03 00 00 00 0X` |
| 设 VDC_TARGET | `FF FF 07 <float>`（0~80V） |
| 设 IAMP_LIMIT | `FF FF 08 <float>`（0~10A） |
| 设 M_LIMIT | `FF FF 09 <float>`（0~0.98） |
| 设 CURRENT_KP / CURRENT_KI | `FF FF 0A / 0B <float>` |
| 设 PLL_KP / PLL_KI | `FF FF 00 / 01 <float>`（1ms 提交） |
| 恢复默认 PLL 参数 | `FE FF 02 00 00 00 00` |
| GET 参数 | `FC FF <ID 0x00~0x0A>`（0x08 已删除，拒绝） → 回 `FD FF <ID> <float>` |

> float 为小端 4 字节。GET 响应会打断一帧 JustFloat（预期行为，自动重同步）。

## 关键 Watch 变量（CCS Expressions）

- **系统/安全**：`g_diagnostics.fast_isr.max_cycles`、`g_diagnostics.system_state`、`g_diagnostics.fault_code`（5=交流过压/6=交流过流/7=直流过压）、`GpioDataRegs.GPADAT.bit.GPIO30`、`.GPBDAT.bit.GPIO42/.GPIO44`
- **PLL**：`g_pll.freq/.vd/.vq/.vmag`、`g_pll_switch_req`、`g_switch_alpha`
- **dq 内环**（X=当前相 0/1/2）：`g_phase_ctrl[X].id/.iq/.id_ref/.iq_ref/.id_err/.iq_err/.id_integral/.iq_integral/.vd_ctrl/.vq_ctrl/.i_alpha/.i_beta/.m_raw/.m`
- **外环**：`g_phase_ctrl[X].vdc_avg/.vdc_ref_ramp/.vdc_integral/.iamp`
- **预充/启动**：`g_run_sup.seq_state`、`g_precharge_vdc_min`、`g_grid_switch_cmd`、`g_bypass_switch_cmd`、`g_start_seq_fail`
- **调试**：`g_jf_lite_mode/.g_jf_enable`、`g_dbg_snap.vline/vac/iac/vdc`、`g_pll_input_vline`（线电压）、`g_pll_input_vabc`（相电压）、`g_vac_vab/vbc/vca_offset_counts`（线电压零偏，看 `g_measurement.vline_v`）
