/* Created by Siquanning */
# 串口 JustFloat 数据说明

> 更新时间：2026-08-17（匹配当前工程：轻量六通道运行时切换 + 完整 VIEW0~10 + 串口 SET/GET 参数协议）


## 1. 串口模式

- 串口：SCI-C（GPIO62/63）
- 波特率：576000 bit/s（实际由 LSPCLK 派生：DEV30≈576923 / TARGET20≈568182）
- 当前 `BOARD_DEBUG_WAVEFORM_LITE=1`：固定 **6 路 `float32` 小端** + 帧尾 `00 00 80 7F`，共 **28 字节**
- 轻量通道组可运行时切换：模式 0=`Va/Vb/Vc/Ia/Ib/Ic`，模式 1=`Vdc1..Vdc6`
- `BOARD_DEBUG_WAVEFORM_LITE=0` 时恢复完整 VIEW0~10：固定 8 路，共 36 字节
- 发送周期：**1 ms（1 kHz）**（50Hz 每周期 20 点，可还原交流波形）
- 上位机：VOFA+，协议 JustFloat（当前轻量模式固定 6 通道，切换通道组无需改配置）
- 发送位置：1 ms 前台任务末尾；**20 kHz 控制 ISR 内不做任何 SCI 发送**
- `BOARD_DEBUG_JUSTFLOAT_ENABLE=1`：JustFloat 生效，Modbus RTU 停用

## 2. 运行期控制变量

| 变量 | 范围 | 默认 | 含义 |
|---|---:|---:|---|
| `g_jf_enable` | 0/1 | 1 | 0=停 JustFloat，1=开 |
| `g_jf_view` | 0..10 | 0 | 完整模式页面选择（下表；轻量模式忽略） |
| `g_jf_phase` | 0..3 | 0 | 观测相：0=自动跟随 `g_ctrl_test_phase`，1=A，2=B，3=C |
| `g_jf_lite_mode` | 0/1 | 0 | 轻量通道组：0=Vac/Iac 六路，1=Vdc1~6 |

四个变量 CCS Expressions 可直接在线改，也可经串口修改（见 §4）。`g_jf_phase` 只影响观测（VIEW2/3/4/5/9 的"当前相"），不影响控制相别锁存。

## 3. 完整模式 VIEW 通道定义（g_jf_view=0..10）

以下 VIEW 仅在 `BOARD_DEBUG_WAVEFORM_LITE=0` 时生效；当前轻量模式请用 `g_jf_lite_mode` 切换两组六通道。

| VIEW | 页面 | CH1 | CH2 | CH3 | CH4 | CH5 | CH6 | CH7 | CH8 |
|---|---|---|---|---|---|---|---|---|---|
| 0 | PLL 跟随 | 实测Va | 实测Vb | 实测Vc | PLL跟随A | PLL跟随B | PLL跟随C | freq | vq |
| 1 | PLL 内部 | Va | Vb | Vc | vd | vq | vmag | freq | lock |
| 2 | 采样 | Ia | Ib | Ic | Va | Vb | Vc | 当前相Vdc1 | 当前相Vdc2 |
| 3 | Vdc 总览 | Vdc1 | Vdc2 | Vdc3 | Vdc4 | Vdc5 | Vdc6 | 当前相VdcAvg | VdcRefRamp |
| 4 | Vdc 外环(当前相) | Vdc1 | Vdc2 | VdcAvg | VdcRefRamp | VdcErr | Iamp | VdcIntegral | Iamp限幅标志 |
| 5 | **dq 内环(当前相)** | Id_ref | Id | Id_err | Iq_ref | Iq | Iq_err | Vd_ctrl | Vq_ctrl |
| 6 | PWM/安全 | m_final | 左桥CMP | 右桥CMP | UNI极性 | GPIO30 | activePhase | TZ状态 | state |
| 7 | 启停 | runReq | state | activePhase | pllLock | GPIO30 | GPIO42 | GPIO44 | firstFault |
| 8 | 综合 | VdcAvg | Vac | Iac | Id_ref | m_final | freq | state | fault |
| 9 | QSG 诊断 | Iac | Ialpha | Ibeta | theta_phase | Id | Iq | freq | activePhase |
| 10 | 线电压 | Vab | Vbc | Vca | vmag | freq | vq | activePhase | pllLock |

通道数据来源：**统一 DebugSnapshot**（`g_dbg_snap`）——20 kHz 控制 ISR 每 20 拍更新一次（1 kHz），1 ms 前台在 DINT 保护下整帧拷贝后发送，保证一帧通道属于同一控制时刻。

### 各页看点

- **VIEW0（默认页）**：实测 Vac 与 PLL 跟随波重合 → 锁相且相序正确；`vq≈0` 锁相误差；`freq≈50`。PLL 无电网输入时 freq=50 且 lock=0（holdover，正常）。
- **VIEW1**：PLL 内部 dq/幅值；`lock=1`（`g_pll_switch_req`）= 已锁定并切到 PLL。
- **VIEW2**：三相电流、三相电压与当前相 Vdc1/2 采样总览。
- **VIEW3**：六路母线全览 + 当前相平均与参考。
- **VIEW4**：电压外环——VdcAvg 追 VdcRefRamp、VdcErr、Iamp（限幅标志=1 表示顶 `g_i_limit_a`）。
- **VIEW5（dq 验收关键页）**：电流与相电压同相时 **Id>0、Iq≈0**；Id_ref 追 Id；Vd/Vq_ctrl 为 dq PI 输出。
- **VIEW6**：m_final；左/右桥 CMP（**65535 = 该桥臂被 AQCSFRC 钳位**，正常斩波时显示真实 CMPA）；UNI 极性；GPIO30；activePhase；TZ 状态；state（0~4）。
- **VIEW7**：启停与故障——runReq、state、activePhase、pllLock、GPIO30/42/44、firstFault（故障码查这里）。
- **VIEW8**：综合巡检一页。
- **VIEW9**：SOGI 虚拟正交轴——Iα/Iβ 应正交（相位差约 90°）、幅值合理。
- **VIEW10**：三路线电压及 PLL 幅值、频率和锁定状态。

## 4. 串口参数协议（SCI-C RX，7 字节定长帧）

帧格式：`[group][0xFF marker][command][payload 4B]`。PARAMS 组 payload 为 `float32 LE`；DEBUG 组的单字节值位于 payload 最后一字节，即 `[00 00 00 value]`。

### SET（修改参数）

**PARAMS 组（group=0xFF）**——float 值，带上下限检查，越界拒绝：

| cmd | 参数 | 写入变量 | 范围 |
|---|---:|---|---|
| 0x00 | PLL_KP | (PLL 暂存→1ms提交) | 0~500 |
| 0x01 | PLL_KI | 同上 | 0~10000 |
| 0x02/03/04 | freq_min/max/nom | 同上 | 40~60Hz |
| 0x05/06 | vq_lock/unlock_ratio | 同上 | 0.001~0.50 |
| 0x07 | VDC_TARGET | `g_vdc_target_v` | 0~600V |
| 0x08 | IAMP_LIMIT | `g_i_limit_a` | 0~100A |
| 0x09 | M_LIMIT | `g_m_limit` | 0~0.98 |
| 0x0A | CURRENT_KP | `g_kp_i` | 0~100 |
| 0x0B | CURRENT_KI | `g_ki_i` | 0~100000 |

**DEBUG 组（group=0xFE）**——单字节值：

| cmd | 参数 | 写入变量 | 范围 |
|---|---:|---|---|
| 0x00 | JF_ENABLE | `g_jf_enable` | 0/1 |
| 0x01 | JF_VIEW | `g_jf_view` | 0~10 |
| 0x02 | 恢复默认 PLL 参数 | — | — |
| 0x03 | JF_PHASE | `g_jf_phase` | 0~3 |
| 0x04 | JF_LITE_MODE | `g_jf_lite_mode` | 0=Vac/Iac，1=Vdc1~6 |

示例：轻量模式切到六路 Vdc → 发 `FE FF 04 00 00 00 01`；切回 Vac/Iac → 发 `FE FF 04 00 00 00 00`。

### GET（读取参数）

请求：`[0xFC][0xFF][参数ID][00 00 00 00]`；响应（DSP→PC）：`[0xFD][0xFF][参数ID][float LE 4B]`

| ID | 参数 | ID | 参数 |
|---|---:|---|---:|
| 0x00 | PLL_KP | 0x05 | CURRENT_KP |
| 0x01 | PLL_KI | 0x06 | CURRENT_KI |
| 0x02 | VDC_TARGET | 0x07 | JF_ENABLE |
| 0x03 | IAMP_LIMIT | 0x08 | JF_VIEW |
| 0x04 | M_LIMIT | 0x09 | JF_PHASE |
| 0x0A | JF_LITE_MODE |  |  |

> 注意：GET 响应会打断一帧 JustFloat 波形（预期行为，下一帧自动重同步）；只读白名单参数，禁止任意 RAM 访问。

## 5. 推荐现场顺序

1. STOP 态：`g_jf_view=0` 确认 PLL 锁定与相序（跟随波重合、freq≈50、vq≈0、lock=1）
2. `g_jf_view=2` 标定采样（零偏/比例）；`g_jf_view=3/4` 看母线
3. 首次 RUN（只放目标相）：`g_jf_view=5` **验收 dq 方向（Id>0、Iq≈0）**；`g_jf_view=9` 看 SOGI 正交轴
4. 异常先看 `g_jf_view=7`（firstFault）；STOP 后再改 `g_ctrl_test_phase` 换相
5. 升压/提限幅前确认 VIEW6 m_final 未持续饱和、VIEW3 六路母线平衡、`fast_isr.max_cycles` 余量

## 6. 当前控制参数默认值（board_config.h）

`g_vdc_target_v=70`、`g_vdc_ramp_rate_vps=10`、`g_i_limit_a=1.5A`、`g_m_limit=0.20`、
`g_kp_v=0.02`、`g_ki_v=2.0`、`g_kp_i=6.0`、`g_ki_i=1200`（**均为初始值，未实机整定**）、`g_rgrid_ohm=0`、`g_power_sign=+1`
