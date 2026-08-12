#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/* ==================================================================
 * BOARD_CONFIG_H — 硬件映射、硬件参数、标定参数与控制边界
 *
 * 组织原则:
 *   1. 参数集中参数化 — 禁止在 .c 文件中硬编码物理量
 *   2. 硬件参数 / 标定参数 / 控制参数分区独立
 *   3. 凡有物理意义的数字必须命名或由公式推导
 *   4. 关键公式写在对应参数区注释中
 *   5. 注释解释"为什么"，不重复代码
 *   6. 模块间不跨层依赖 (驱动←BSP←Service←Control)
 * ================================================================== */

/* ================= 通信 / 定时 ================= */

#define BOARD_SYSCLK_MHZ         140U   /* Y1 = 20 MHz, PLL ×7 /1 */
#define BOARD_LSPCLK_HZ          35000000U  /* SYSCLK=140MHz, LOSPCP=/4 */
#define BOARD_SCI_BAUD           9600UL
#define BOARD_SCI_BRR             455U  /* 35000000/(9600*8)-1 = 455, actual 9594 */
#define BOARD_SPIA_BRR             127U
#define BOARD_TIMER0_PERIOD_US     100U

/* ---- 硬件确认闸 ----
 * 所有 PWM/ADC 外设操作受此闸保护，置 0 可切断全部功率输出路径。
 */
#define BOARD_PWM_ADC_HW_CONFIRMED   1U

/* ================= ADC ================= */

#define BOARD_ADC_ENABLED           1U

#define BOARD_VDC_CHANNEL_COUNT     6U
#define BOARD_VAC_CHANNEL_COUNT     3U
#define BOARD_IAC_CHANNEL_COUNT     3U
#define BOARD_ADC_CH_COUNT          (BOARD_VDC_CHANNEL_COUNT + \
                                     BOARD_VAC_CHANNEL_COUNT + \
                                     BOARD_IAC_CHANNEL_COUNT)

/*
 * ADC 通道映射 — 12 路 SEQ1 级联采样，由 EPWM1 SOCA 触发 (CTR=ZERO, 20kHz)
 *
 *   CONV00..05 → ADCRESULT0..5  → Vdc1..Vdc6   (ADCINB6,B7,B4,B5,B2,B3)
 *   CONV06..08 → ADCRESULT6..8  → Va / Vb / Vc  (ADCINB1, ADCINB0, ADCINA0)
 *   CONV09..11 → ADCRESULT9..11 → Ia / Ib / Ic  (ADCINA7, ADCINA6, ADCINA5)
 */
#define BOARD_ADC_CONV00            0xEU    /* ADCINB6 → Vdc1 */
#define BOARD_ADC_CONV01            0xFU    /* ADCINB7 → Vdc2 */
#define BOARD_ADC_CONV02            0xCU    /* ADCINB4 → Vdc3 */
#define BOARD_ADC_CONV03            0xDU    /* ADCINB5 → Vdc4 */
#define BOARD_ADC_CONV04            0xAU    /* ADCINB2 → Vdc5 */
#define BOARD_ADC_CONV05            0xBU    /* ADCINB3 → Vdc6 */
#define BOARD_ADC_CONV06            0x9U    /* ADCINB1 → Va */
#define BOARD_ADC_CONV07            0x8U    /* ADCINB0 → Vb */
#define BOARD_ADC_CONV08            0x0U    /* ADCINA0 → Vc */
#define BOARD_ADC_CONV09            0x7U    /* ADCINA7 → Ia */
#define BOARD_ADC_CONV10            0x6U    /* ADCINA6 → Ib */
#define BOARD_ADC_CONV11            0x5U    /* ADCINA5 → Ic */

/*
 * ADC 时钟 — HSPCLK=75MHz, ADCCLKPS=3 → ADCCLK=12.5MHz
 * ACQ_PS=7 → 采样窗口 = 8 ADCCLK 周期
 */
#define BOARD_ADC_ACQ_PS            7U
#define BOARD_ADC_CPS               0U
#define BOARD_ADC_ADCCLKPS          3U

/*
 * ADC common hardware parameters — shared across all analog input channels.
 * F28335 internal reference: 3.0 V, 12-bit resolution → 4096 codes.
 */
#define BOARD_ADC_VREF_V                3.0f
#define BOARD_ADC_MAX_COUNT           4095.0f

/* ================= PWM ================= */

/*
 * 架构: 3 相 H 桥 → 6 个 ePWM 模块 (每相左右半桥各一)
 * 每个模块使用 CMPA 控制，A/B 互补输出，死区由 DSP Dead-Band 模块生成
 */
#define BOARD_PHASE_COUNT            3U
#define BOARD_EPWM_MODULE_COUNT      6U

/*
 * 诊断锚点模块 — ADC ISR 和状态机通过此模块进行 TZ 轮询和故障检测
 */
#define BOARD_EPWM_MODULE           1U

/* ---- 时序 ---- */
#define BOARD_PWM_FREQ_HZ           20000U
/* 计数模式: 0=UP, 1=DOWN, 2=UPDOWN (TI TBCTL.CTRMODE) */
#define BOARD_PWM_COUNT_MODE        2U

/* DSP 死区时间 (ns)，设 0 关闭 DSP 死区 */
#define BOARD_PWM_DEADTIME_NS       1000U   /* 1.0 µs */
/* DBRED/DBFED = deadtime_ns × SYSCLK_MHZ / 1000 */
#define BOARD_PWM_DB_RED  ((BOARD_PWM_DEADTIME_NS * BOARD_SYSCLK_MHZ) / 1000U)
#define BOARD_PWM_DB_FED  ((BOARD_PWM_DEADTIME_NS * BOARD_SYSCLK_MHZ) / 1000U)

#if (BOARD_PWM_DB_RED > 1023U) || (BOARD_PWM_DB_FED > 1023U)
#error "ePWM dead-band count exceeds 10-bit DBRED/DBFED range (max 1023)"
#endif

/* TBPRD = TBCLK / (2 × f_pwm),  TBCLK = SYSCLK = 140MHz */
#define BOARD_PWM_TBPRD  ((BOARD_SYSCLK_MHZ * 1000000UL) \
                         / (2UL * BOARD_PWM_FREQ_HZ))

/*
 * 初始占空比 — 仅用于 board.c 初始化阶段写入 CMPA，
 * 不参与运行时调制限幅计算。
 */
#define BOARD_PWM_FIXED_DUTY_PERMILL 100U

/*
 * 钳位式单极性调制边界 (Control_ComputeModulation 使用)
 *
 * m > 0 → A 腿钳位 HIGH, B 腿斩波: cmpb = TBPRD * (1000 - m) / 1000
 * m < 0 → B 腿钳位 HIGH, A 腿斩波: cmpa = TBPRD * (1000 + m) / 1000
 * m = 0 → 双钳 HIGH (零电压状态)
 *
 * CMP_MIN/MAX 只限制斩波腿；钳位腿通过 AQCSFRC 强制 HIGH。
 */
#define BOARD_MODULATION_DUTY_MIN_PERMILL   10U   /*  1.0% (m ≈ -0.98) */
#define BOARD_MODULATION_DUTY_MAX_PERMILL  990U   /* 99.0% (m ≈ +0.98) */
#define BOARD_MODULATION_CMP_MIN  ((BOARD_PWM_TBPRD * (uint32_t)BOARD_MODULATION_DUTY_MIN_PERMILL) / 1000U)
#define BOARD_MODULATION_CMP_MAX  ((BOARD_PWM_TBPRD * (uint32_t)BOARD_MODULATION_DUTY_MAX_PERMILL) / 1000U)

/* ==================================================================
 * 三类采样硬件链 — IAC / VAC / VDC
 *
 * 统一结构:
 *   Physical quantity → CT1 → CT2 → analog conditioning → ADC
 *
 * CT1 当前暂定 1:1，但保留独立宏，后续改 CT1 参数不需修改换算代码。
 * CT2 / 跨阻 / 增益为实际硬件参数。
 *
 * 换算公式统一从硬件参数推导，禁止手填最终 gain 魔法数字。
 *
 * Offset / Polarity 独立作为 Calibration Defaults，与 CT 参数分离。
 * ================================================================== */

/* ================= IAC Sense ================= */

/*
 * 电流采样链 (analog signal path):
 *
 *   I_primary [A]
 *     → CT1:  I_s1 = I_primary × (CT1_SEC_A / CT1_PRI_A)
 *     → CT2:  I_s2 = I_s1 × (CT2_SEC_A / CT2_PRI_A)
 *     → R120: V_tia = I_s2 × TIA_OHM                              [V]
 *     → ×½:   V_adc_ac = V_tia × ANALOG_GAIN                      [V]
 *     → ADC:  V_adc = V_bias ± V_adc_ac                           [V]
 *
 *   ADC raw → primary current:
 *
 *     I_primary [A] = POLARITY × (raw - OFFSET) × IAC_GAIN
 *
 *     where IAC_GAIN = (CT1_PRI_A / CT1_SEC_A)
 *                    × (CT2_PRI_A / CT2_SEC_A)
 *                    / TIA_OHM / ANALOG_GAIN
 *                    × (VREF_V / MAX_COUNT)
 */

/* ---- IAC CT1: 贯穿式电流互感器 (调试阶段 1:1) ---- */
#define BOARD_IAC_CT1_PRI_A          1.0f
#define BOARD_IAC_CT1_SEC_A          1.0f

/* ---- IAC CT2: 控制板载精密电流互感器 ---- */
#define BOARD_IAC_CT2_PRI_A          5.0f
#define BOARD_IAC_CT2_SEC_A          0.0025f

/* ---- IAC 跨阻放大器反馈电阻 R120 (实装值) ---- */
#define BOARD_IAC_TIA_OHM           1000.0f

/* ---- IAC 后级模拟增益 (≈1/2 分压) ---- */
#define BOARD_IAC_ANALOG_GAIN          0.5f

/* ---- IAC 偏置电压 (ADC 1.5V 中点) ---- */
#define BOARD_IAC_BIAS_V               1.5f

/* ---- IAC 标定默认值（待实机标定） ---- */
#define BOARD_IAC_IA_OFFSET_COUNTS   2048U
#define BOARD_IAC_IB_OFFSET_COUNTS   2048U
#define BOARD_IAC_IC_OFFSET_COUNTS   2048U

#define BOARD_IAC_IA_POLARITY        (+1.0f)
#define BOARD_IAC_IB_POLARITY        (+1.0f)
#define BOARD_IAC_IC_POLARITY        (+1.0f)

/* ================= VAC Sense ================= */

/*
 * 电压采样链 (analog signal path):
 *
 *   V_primary [Vrms]
 *     → CT1:  V_s1 = V_primary × (CT1_SEC_V / CT1_PRI_V)
 *     → CT2:  I_s2 = V_s1 × (CT2_SEC_A / CT2_PRI_V)              [A]
 *     → R28:  V_tia = I_s2 × TIA_OHM                              [V]
 *     → ×½:   V_adc_ac = V_tia × ANALOG_GAIN                      [V]
 *     → ADC:  V_adc = V_bias ± V_adc_ac                           [V]
 *
 *   ADC raw → primary voltage:
 *
 *     V_primary [Vrms] = (raw - OFFSET) × VAC_GAIN
 *
 *     where VAC_GAIN = (CT1_PRI_V / CT1_SEC_V)
 *                    × (CT2_PRI_V / CT2_SEC_A)
 *                    / TIA_OHM / ANALOG_GAIN
 *                    × (VREF_V / MAX_COUNT)
 */

/* ---- VAC CT1: 电压互感器 (调试阶段 1:1) ---- */
#define BOARD_VAC_CT1_PRI_V_RMS      1.0f
#define BOARD_VAC_CT1_SEC_V_RMS      1.0f

/*
 * ---- VAC CT2: 电压→电流变换 ----
 * 实装器件: 100Vrms → 1.67mArms.
 * NOTE: 原理图旧标注 "2mA / ±2.82V" 为设计初期估算值，不用于当前软件换算。
 * 所有 Vac 换算均以本文件 BOARD_VAC_CT2_SEC_A_RMS = 0.00167f 为唯一来源。
 */
#define BOARD_VAC_CT2_PRI_V_RMS    100.0f
#define BOARD_VAC_CT2_SEC_A_RMS      0.00167f

/* ---- VAC 跨阻放大器反馈电阻 R28 (实装值) ---- */
#define BOARD_VAC_TIA_OHM           1000.0f

/* ---- VAC 后级模拟增益 (≈1/2 分压) ---- */
#define BOARD_VAC_ANALOG_GAIN          0.5f

/* ---- VAC 偏置电压 (ADC 1.5V 中点) ---- */
#define BOARD_VAC_BIAS_V               1.5f

/* ---- VAC 标定默认值（待实机标定） ---- */
#define BOARD_VAC_VA_OFFSET_COUNTS   2048U
#define BOARD_VAC_VB_OFFSET_COUNTS   2048U
#define BOARD_VAC_VC_OFFSET_COUNTS   2048U

#define BOARD_VAC_VA_POLARITY        (+1.0f)
#define BOARD_VAC_VB_POLARITY        (+1.0f)
#define BOARD_VAC_VC_POLARITY        (+1.0f)

/* ================= VDC Sense ================= */

/*
 * DC 电压采样链 (analog signal path):
 *
 *   V_primary [V]
 *     → CT1:  V_s1 = V_primary × (CT1_SEC_V / CT1_PRI_V)
 *     → CT2:  V_s2 = V_s1 × (CT2_SEC_V / CT2_PRI_V)
 *     → buffer: V_adc = V_s2 × ANALOG_GAIN                        [V]
 *     → ADC:   direct (no bias offset)
 *
 *   ADC raw → primary voltage:
 *
 *     V_primary [V] = raw × VDC_GAIN
 *
 *     where VDC_GAIN = (CT1_PRI_V / CT1_SEC_V)
 *                    × (CT2_PRI_V / CT2_SEC_V)
 *                    / ANALOG_GAIN
 *                    × (VREF_V / MAX_COUNT)
 */

/* ---- VDC CT1: 分压网络 (调试阶段 1:1) ---- */
#define BOARD_VDC_CT1_PRI_V          1.0f
#define BOARD_VDC_CT1_SEC_V          1.0f

/* ---- VDC CT2: 隔离放大器 (调试阶段 1:1) ---- */
#define BOARD_VDC_CT2_PRI_V          1.0f
#define BOARD_VDC_CT2_SEC_V          1.0f

/* ---- VDC 模拟缓冲增益 (unity) ---- */
#define BOARD_VDC_ANALOG_GAIN          1.0f

/* ================= Trip Zone ================= */

/*
 * TZSEL 位掩码: bit0-5=CBC1-6, bit8-13=OSHT1-6
 * TZ1+TZ2 单次触发 (OSHT) 可强制所有 PWM 输出 LOW
 */
#define BOARD_TZ_OSHT_SOURCES   ((1U << 8) | (1U << 9))
#define BOARD_TZ_CBC_SOURCES    0U                          /* 暂不使用逐周期限流 */

/*
 * TZ input active level (verified against schematic):
 *   TZ1 = GPIO12, active-low  (0 = fault, 1 = safe)
 *   TZ2 = GPIO13, active-low  (0 = fault, 1 = safe)
 */
#define BOARD_TZ1_ACTIVE_LEVEL  0U
#define BOARD_TZ2_ACTIVE_LEVEL  0U

/* ================= CPLD SPI ================= */

/*
 * CPLD SPI 协议验证开关 — 临时调试用。
 * 置 1 时每 100ms 发送固定 ±200‰ 调制 + COMMIT 帧。
 * SignalTap 确认 CPLD 寄存器正确后置 0。
 */
#define BOARD_CPLD_PROTOCOL_TEST    0U

#endif
