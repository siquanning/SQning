# 串口 JustFloat 数据说明

## 1. 当前串口模式

- 串口：SCIC
- 波特率：115200 bit/s
- 数据格式：8 路 `float32` 小端数据，随后发送帧尾 `00 00 80 7F`
- 每帧长度：36 字节
- 发送周期：100 ms（10 Hz）
- 上位机：VOFA+，协议选择 JustFloat
- 当前 `BOARD_DEBUG_JUSTFLOAT_ENABLE=1`，因此 Modbus RTU 轮询停用，二者不会同时占用串口。

10 Hz用于观察慢趋势、锁定状态和限幅情况，不能还原50 Hz交流波形细节。

## 2. CCS现场控制变量

| 变量 | 数值 | 含义 |
|---|---:|---|
| `g_debug_view` | 1 | PLL视图 |
| `g_debug_view` | 2 | 当前测试相双闭环视图 |
| `g_ctrl_test_phase` | 1 | A相测试 |
| `g_ctrl_test_phase` | 2 | B相测试 |
| `g_ctrl_test_phase` | 3 | C相测试 |

`g_debug_view`可在线切换。`g_ctrl_test_phase`只在一次启动开始时锁存，RUN期间修改不会热切换PWM相别；换相必须STOP后修改，再重新启动。

## 3. PLL视图：`g_debug_view=1`

| 通道 | 变量 | 单位 | 用途 |
|---:|---|---|---|
| CH0 | `theta` | rad | PLL电角度，正常范围0～2π |
| CH1 | `freq` | Hz | PLL估算频率 |
| CH2 | `vd` | V | d轴电压，锁定时应接近`vmag`且为正 |
| CH3 | `vq` | V | q轴误差，锁定后应接近0 |
| CH4 | `vmag` | V峰值 | Clarke αβ电压幅值 |
| CH5 | `pll_i` | rad/s | PLL积分项 |
| CH6 | `alpha` | 1 | LUT到PLL淡入系数，0～1 |
| CH7 | 相位误差 | ° | PLL目标相位与当前调制相位之差 |

判断PLL可以接管时，重点看：`freq=49.5～50.5 Hz`、`|vq|<3%×vmag`、`vd>0.9×vmag`、`alpha≈1`。

## 4. 双闭环视图：`g_debug_view=2`

| 通道 | 变量 | 单位 | 用途 |
|---:|---|---|---|
| CH0 | 当前相`Vac` | V瞬时值 | A/B/C由锁存测试相自动选择 |
| CH1 | 当前相`Iac` | A瞬时值 | 实际交流电流 |
| CH2 | `Iref` | A瞬时值 | 电流内环参考 |
| CH3 | `Vdc_avg` | V | 当前相两路直流电压平均值 |
| CH4 | `Vdc_ref_ramp` | V | 电压外环斜坡参考 |
| CH5 | `Iamp` | A峰值 | 电压PI输出的电流幅值，当前限幅0～0.5 A |
| CH6 | `m` | 1 | 调制量，当前限幅-0.20～+0.20 |
| CH7 | `theta_phase` | rad | 当前测试相使用的相位角 |

重点观察：CH1是否跟随CH2、CH3是否跟随CH4、CH5是否长时间顶到0.5 A、CH6是否长时间顶到±0.20。

## 5. 推荐现场顺序

1. STOP状态设置`g_ctrl_test_phase=1`、`g_debug_view=1`，先确认PLL。
2. PLL稳定后设置`g_debug_view=2`，观察A相闭环。
3. A相完成后STOP，再设置测试相2或3重新启动。
4. 出现`alpha`下降、`m`饱和、电流方向相反或Vdc异常时，先STOP，不在RUN中修改测试相。
