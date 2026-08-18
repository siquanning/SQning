/* Created by Siquanning */
# 串口 JustFloat 数据说明

> 更新时间：2026-08-18（lite 0=线电压+电流，1=Vdc，2=相电压+PLL跟随）


## 1. 串口模式

- 串口：SCI-C（GPIO62/63）
- 波特率：576000 bit/s（实际由 LSPCLK 派生：DEV30≈576923 / TARGET20≈568182）
- 固定 **6 路 `float32` 小端** + 帧尾 `00 00 80 7F`，共 **28 字节**
- 通道组用 `g_jf_lite_mode` 运行时切换：0=线电压+电流，1=Vdc1..Vdc6，2=相电压+PLL跟随波
- 发送周期：**1 ms（1 kHz）**（50Hz 每周期 20 点，可还原交流波形）
- 上位机：VOFA+，协议 JustFloat（固定 6 通道，切换通道组无需改 VOFA 配置）
- 发送位置：1 ms 前台任务末尾；**20 kHz 控制 ISR 内不做任何 SCI 发送**
- `BOARD_DEBUG_JUSTFLOAT_ENABLE=1`：JustFloat 生效，Modbus RTU 停用

## 2. 运行期控制变量

| 变量 | 范围 | 默认 | 含义 |
|---|---:|---:|---|
| `g_jf_enable` | 0/1 | 1 | 0=停 JustFloat，1=开 |
| `g_jf_lite_mode` | 0/1/2 | 0 | 0=Vab/Vbc/Vca+Ia/Ib/Ic，1=Vdc1~6，2=Va/Vb/Vc+PLL跟随波 |
| `g_jf_phase` | 0..3 | 0 | 观测相（协议保留；当前六通道组不使用） |

CCS Expressions 可直接在线改，也可经串口修改（见 §4）。

## 3. 轻量通道组

| mode | CH1 | CH2 | CH3 | CH4 | CH5 | CH6 |
|---|---|---|---|---|---|---|
| 0 | Vab | Vbc | Vca | Ia | Ib | Ic |
| 1 | Vdc1 | Vdc2 | Vdc3 | Vdc4 | Vdc5 | Vdc6 |
| 2 | Va | Vb | Vc | PLL跟随A | PLL跟随B | PLL跟随C |

- **mode 0** 的 Vab/Vbc/Vca 是 ADC 实测**线电压**（`g_pll_input_vline`）。
- **mode 2** 的 Va/Vb/Vc 是线→相重构后的**相电压**（`g_pll_input_vabc` / PLL 输入）；跟随波为 `vmag·cos(θ±0/120/240°)`，锁定且相序正确时应与三相相电压重合。
- 线电压也可在 CCS 看 `g_measurement.vline_v[0..2]`。
- dq 看 CCS：`g_phase_ctrl[X].*`。

通道数据来源：**统一 DebugSnapshot**（`g_dbg_snap`）——20 kHz 控制 ISR 每 20 拍更新一次（1 kHz），1 ms 前台在 DINT 保护下整帧拷贝后发送。

### 现场看点

- **mode 0**：三路线电压相差 120°、电流同相序。
- **mode 1**：六路母线。
- **mode 2**：相电压与三路跟随波重合、相序正确；再在 CCS 看 `g_pll.freq≈50`、`vq≈0`、`g_pll_switch_req=1`。
- dq 验收：CCS 看电流与**相电压**同相时 **Id>0、Iq≈0**。

## 4. 串口参数协议（SCI-C 576000）

帧格式固定 7 字节：`[group][0xFF][cmd/id][payload 4B]`。payload 为 float32 小端（DEBUG 组只用最后 1 字节）。

### SET（写入参数）

**PARAMS 组（group=0xFF）**——float 值：

| cmd | 参数 | 写入变量 | 范围 |
|---|---:|---|---|
| 0x00 | PLL_KP | 暂存，1ms 提交 | 见 board_config.h |
| 0x01 | PLL_KI | 暂存，1ms 提交 | 见 board_config.h |
| 0x07 | VDC_TARGET | `g_vdc_target_v` | 0~600 |
| 0x08 | IAMP_LIMIT | `g_i_limit_a` | 0~100 |
| 0x09 | M_LIMIT | `g_m_limit` | 0~0.98 |
| 0x0A | CURRENT_KP | `g_kp_i` | 0~100 |
| 0x0B | CURRENT_KI | `g_ki_i` | 0~100000 |

**DEBUG 组（group=0xFE）**——单字节值：

| cmd | 参数 | 写入变量 | 范围 |
|---|---:|---|---|
| 0x00 | JF_ENABLE | `g_jf_enable` | 0/1 |
| 0x01 | （已删除） | — | 拒绝 |
| 0x02 | 恢复默认 PLL 参数 | — | — |
| 0x03 | JF_PHASE | `g_jf_phase` | 0~3 |
| 0x04 | JF_LITE_MODE | `g_jf_lite_mode` | 0=线电压+Iac，1=Vdc，2=相电压+PLL跟随 |

示例：切到 PLL 跟随 → 发 `FE FF 04 00 00 00 02`；切到六路 Vdc → `FE FF 04 00 00 00 01`；切回线电压 → `FE FF 04 00 00 00 00`。

### GET（读取参数）

请求：`[0xFC][0xFF][参数ID][00 00 00 00]`；响应（DSP→PC）：`[0xFD][0xFF][参数ID][float LE 4B]`

| ID | 参数 | ID | 参数 |
|---|---:|---|---:|
| 0x00 | PLL_KP | 0x05 | CURRENT_KP |
| 0x01 | PLL_KI | 0x06 | CURRENT_KI |
| 0x02 | VDC_TARGET | 0x07 | JF_ENABLE |
| 0x03 | IAMP_LIMIT | 0x08 | （已删除，拒绝） |
| 0x04 | M_LIMIT | 0x09 | JF_PHASE |
| 0x0A | JF_LITE_MODE |  |  |

> 注意：GET 响应会打断一帧 JustFloat 波形（预期行为，下一帧自动重同步）；只读白名单参数，禁止任意 RAM 访问。

## 5. 推荐现场顺序

1. STOP 态：lite mode 0 看线电压；mode 2 看相电压与 PLL 跟随波重合；CCS 确认 `freq≈50`、`vq≈0`、`lock=1`
2. 标定采样（线电压 Vab/Vbc/Vca 零偏/比例、Vdc 锚点）；lite mode 0 看线电压校零，lite mode 1 看六路母线；禁止用 mode 2 调零偏
3. 首次 RUN（只放目标相）：CCS 验收 **Id>0、Iq≈0**；看 `g_phase_ctrl[X].i_alpha/i_beta`
4. 异常先看 `g_diagnostics.fault_code` / `g_dbg_snap` 之外的状态机变量；STOP 后再改 `g_ctrl_test_phase`
5. 升压前确认 `g_phase_ctrl[X].m` 未持续饱和、六路母线平衡、`fast_isr.max_cycles` 余量

## 6. 当前控制参数默认值（board_config.h）

`g_vdc_target_v=70`、`g_vdc_ramp_rate_vps=10`、`g_i_limit_a=1.5A`、`g_m_limit=0.20`、
`g_kp_v=0.02`、`g_ki_v=2.0`、`g_kp_i=6.0`、`g_ki_i=1200`（**均为初始值，未实机整定**）、`g_rgrid_ohm=0`、`g_power_sign=+1`
