/* Created by Siquanning */
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
 *
 * 时钟策略（双 Board Clock Profile）:
 *   - 本文件只声明「功能目标」；时钟根参数与所有寄存器值派生
 *     统一放在 board_clock_profile.h（OSCCLK/PLLCR/DIVSEL → SYSCLK →
 *     HSPCLK/LSPCLK/TBCLK → TBPRD/DBRED/Timer0/BRR）。
 *   - 两个 Profile（TARGET_20MHZ / DEV_30MHZ）必须产生相同的
 *     控制系统真实时间行为（PWM=20kHz, Ts=50us, deadtime=1us,
 *     Timer0=100us, scheduler=1/10/100ms, SCI=576000, SPI≈1MHz）。
 * ================================================================== */

#include "firmware/bsp/board_clock_profile.h"

/* ================= 通信 / 定时 ================= */

/* 时钟根参数与派生值全部来自 board_clock_profile.h（不再在此硬编码）。
 * 直接使用：BOARD_OSCCLK_HZ / BOARD_PLLCR / BOARD_DIVSEL / BOARD_SYSCLK_HZ /
 * BOARD_HSPCLK_HZ / BOARD_LSPCLK_HZ / BOARD_EPWM_TBCLK_HZ / BOARD_PWM_TBPRD /
 * BOARD_PWM_DB_RED / BOARD_PWM_DB_FED / BOARD_TIMER0_PRD / BOARD_SCI_BRR /
 * BOARD_SPI_BRR / BOARD_FAST_ISR_CYCLE_BUDGET 等。 */
#define BOARD_SYSCLK_MHZ         (BOARD_SYSCLK_HZ / 1000000UL)
#define BOARD_SCI_BRR_PROFILE    BOARD_SCI_BRR     /* Profile 派生的 BRR（DrvSci 实际自算，宏仅作记录/检查） */
#define BOARD_SPIA_BRR           BOARD_SPI_BRR     /* Profile 派生的 SPI BRR */

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
 *   CONV06..08 → ADCRESULT6..8  → Vab / Vbc / Vca  (ADCINB1, ADCINB0, ADCINA0)
 *   CONV09..11 → ADCRESULT9..11 → Ia / Ib / Ic  (ADCINA7, ADCINA6, ADCINA5)
 *
 * Vac 三路硬件采的是线电压。ConvertVac 输出 Vab/Vbc/Vca，
 * Measurement_LineToPhase 重构 Va/Vb/Vc 后再进 PLL 与闭环前馈。
 */
#define BOARD_ADC_CONV00            0xEU    /* ADCINB6 → Vdc1 */
#define BOARD_ADC_CONV01            0xFU    /* ADCINB7 → Vdc2 */
#define BOARD_ADC_CONV02            0xCU    /* ADCINB4 → Vdc3 */
#define BOARD_ADC_CONV03            0xDU    /* ADCINB5 → Vdc4 */
#define BOARD_ADC_CONV04            0xAU    /* ADCINB2 → Vdc5 */
#define BOARD_ADC_CONV05            0xBU    /* ADCI NB3 → Vdc6 */
#define BOARD_ADC_CONV06            0x9U    /* ADCINB1 → Vab */
#define BOARD_ADC_CONV07            0x8U    /* ADCINB0 → Vbc */
#define BOARD_ADC_CONV08            0x0U    /* ADCINA0 → Vca */
#define BOARD_ADC_CONV09            0x7U    /* ADCINA7 → Ia */
#define BOARD_ADC_CONV10            0x6U    /* ADCINA6 → Ib */
#define BOARD_ADC_CONV11            0x5U    /* ADCINA5 → Ic */

/*
 * ADC 时钟 — 由 board_clock_profile.h 派生：
 *   ADCCLK = HSPCLK / (ADCCLKPS × 2)（CPS=0）
 *   TARGET_20MHZ(100MHz):  HSPCLK=50MHz, ADCCLKPS=3 → ADCCLK≈8.33MHz
 *   DEV_30MHZ(120MHz):    HSPCLK=60MHz, ADCCLKPS=3 → ADCCLK=10.00MHz
 * ACQ_PS=7 → 采样窗口 = 8 ADCCLK 周期
 * 注：BOARD_ADC_ADCCLKPS / BOARD_ADC_CPS / BOARD_ADC_ACQ_PS 定义于
 *     board_clock_profile.h（两个 Profile 共用相同分频与采样窗配置）。
 */

/*
 * ADC公共硬件标定参数，供所有模拟量换算使用。
 * BOARD_ADC_VREF_V：ADC参考电压，单位V。
 * BOARD_ADC_MAX_COUNT：12位ADC最大码值。
 * 这两个参数属于测量标定，不是软起动门槛；修改前必须确认DSP参考源和ADC格式。
 */
/* ADC参考电压，单位V；属于测量标定，修改前必须确认F28335实际参考源。 */
#define BOARD_ADC_VREF_V                3.0f

/* 12位ADC的最大输出码值；用于raw到电压的换算，不是采样通道数量。 */
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

/* ---- 时序 ----
 * 功能目标与派生值统一由 board_clock_profile.h 提供：
 *   BOARD_PWM_FREQ_HZ=20000, BOARD_CONTROL_TS=1/20000=50us,
 *   BOARD_PWM_COUNT_MODE=2(up-down), BOARD_PWM_DEADTIME_NS=1000(1us),
 *   BOARD_PWM_TBPRD, BOARD_PWM_DB_RED/FED 由 TBCLK 推导。
 * 两个 Profile 产生完全相同的 PWM=20kHz / deadtime=1us / Ts=50us。
 */
#if (BOARD_PWM_DB_RED > 1023U) || (BOARD_PWM_DB_FED > 1023U)
#error "ePWM dead-band count exceeds 10-bit DBRED/DBFED range (max 1023)"
#endif

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
 * 当前低压台架测试中，Vdc/Vac/Iac三类CT1均按1:1换算。
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

/*
 * Iac 前级电流互感器 CT1：一次 100A / 二次 5A（100:5）。
 * 单位：A。该参数属于硬件测量标定，禁止通过修改它调整控制门槛。
 */
#define BOARD_IAC_CT1_PRI_A          100.0f
#define BOARD_IAC_CT1_SEC_A          5.0f

/* ---- IAC CT2: 控制板载精密电流互感器 ---- */
#define BOARD_IAC_CT2_PRI_A          5.0f
#define BOARD_IAC_CT2_SEC_A          0.0025f

/* ---- IAC 跨阻放大器反馈电阻 R120 (实装值) ---- */
#define BOARD_IAC_TIA_OHM           1000.0f

/* ---- IAC 后级模拟增益 (≈1/2 分压) ---- */
#define BOARD_IAC_ANALOG_GAIN          0.5f

/* ---- IAC 偏置电压 (ADC 1.5V 中点) ---- */
#define BOARD_IAC_BIAS_V               1.5f

/*
 * IAC各通道ADC零偏上电默认值，单位ADC count；现场临时修改
 * g_iac_ix_offset_counts。零偏只修正零输入读数，禁止用来补偿比例误差。
 */
#define BOARD_IAC_IA_OFFSET_COUNTS_DEFAULT 2065U
#define BOARD_IAC_IB_OFFSET_COUNTS_DEFAULT 2070U
#define BOARD_IAC_IC_OFFSET_COUNTS_DEFAULT 2053U

#define BOARD_IAC_IA_POLARITY        (+1.0f)
#define BOARD_IAC_IB_POLARITY        (+1.0f)
#define BOARD_IAC_IC_POLARITY        (+1.0f)

/* ================= VAC Sense ================= */

/*
 * 电压采样链 (analog signal path):
 *
 *   V_primary [Vrms]  （三路 CT 一次侧为线电压 Vab/Vbc/Vca）
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

/*
 * 当前低压台架测试：Vac CT1按1:1换算。
 * 单位：Vrms。该参数属于硬件测量标定，禁止通过修改它调整PLL门槛。
 */
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

/*
 * VAC 线电压通道 ADC 零偏上电默认值，单位 ADC count。
 * 现场临时修改 g_vac_vab/vbc/vca_offset_counts；看 g_measurement.vline_v，
 * 禁止对着重构相电压 vac_v / JustFloat mode 2 调零偏。
 * 零偏只修正零输入读数，禁止用来补偿比例误差。
 */
#define BOARD_VAC_VAB_OFFSET_COUNTS_DEFAULT 2023U
#define BOARD_VAC_VBC_OFFSET_COUNTS_DEFAULT 2040U
#define BOARD_VAC_VCA_OFFSET_COUNTS_DEFAULT 2559U

#define BOARD_VAC_VAB_POLARITY       (+1.0f)
#define BOARD_VAC_VBC_POLARITY       (+1.0f)
#define BOARD_VAC_VCA_POLARITY       (+1.0f)

/* ============================================================
 * Vdc采样标定参数
 * ============================================================ */

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

/*
 * Vdc采样一级变换CT1的一次侧额定电压，单位V。
 * 当前低压台架测试按1V:1V换算。
 * 该宏属于Measurement硬件标定，禁止用它调整软起动完成门槛。
 */
#define BOARD_VDC_CT1_PRI_V          1000.0f
/*
 * Vdc采样一级变换CT1的二次侧额定电压，单位V。
 * 当前低压台架测试按1V:1V换算。
 */
#define BOARD_VDC_CT1_SEC_V          2.0f

/*
 * Vdc采样二级变换CT2的一次侧额定电压，单位V。
 * 当前1.0表示隔离放大环节暂按1:1计算；修改前必须确认器件型号和接法。
 */
#define BOARD_VDC_CT2_PRI_V          1.0f

/*
 * Vdc采样二级变换CT2的二次侧额定电压，单位V。
 * 与CT2_PRI共同决定隔离环节比例，属于硬件标定，不是启动实验参数。
 */
#define BOARD_VDC_CT2_SEC_V          1.0f

/*
 * CT2之后至ADC输入之间按1:1进入ADC，模拟增益为1.0。
 * 修改前必须依据实际模拟调理电路确认，Measurement层将统一使用该值换算。
 */
#define BOARD_VDC_ANALOG_GAIN          1.0f

/* ============================================================
 * Vdc各通道ADC零偏上电默认值
 *
 * 单位均为ADC count，每路独立保存。当前默认0表示Vdc链暂按无偏置输入处理。
 * 现场只能用对应g_vdcN_offset_counts修正零输入时的ADC残余码值；
 * 禁止通过offset补偿CT1、CT2、模拟增益或ADC参考电压的比例误差。
 * DSP复位后，运行变量会重新加载以下DEFAULT值。
 * ============================================================ */

#define BOARD_VDC1_OFFSET_COUNTS_DEFAULT      0U
#define BOARD_VDC2_OFFSET_COUNTS_DEFAULT      0U
#define BOARD_VDC3_OFFSET_COUNTS_DEFAULT      0U
#define BOARD_VDC4_OFFSET_COUNTS_DEFAULT      0U
#define BOARD_VDC5_OFFSET_COUNTS_DEFAULT      0U
#define BOARD_VDC6_OFFSET_COUNTS_DEFAULT      0U

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

/* ================= 启停控制 (Run/Stop) ================= */

/*
 * 启动流程选择：
 * 0 = 常规预充（不控整流软启动）：先合GPIO42（S1~S3，预充电阻串入）经反并联
 *     二极管不控整流给母线充电，Vdc≥g_precharge_done_v 后合GPIO44（S4~S6旁路
 *     预充电阻），再等 g_bypass_delay_ms + PLL/TZ 就绪才释放PWM。
 *     —— 高压（220/450Vac）必须用此模式，避免直接合闸冲击电流。
 * 1 = 低压直测：跳过预充，GPIO42/44同开同关（仅低压台架验证用，当前）。
 */
#define BOARD_LOW_VOLTAGE_DIRECT_TEST      1U

/*
 * PLL/继电器台架测试模式：
 * 1 = 台架模式：20kHz ISR只运行采样和PLL，不执行双闭环；ePWM恒OST、
 *     GPIO30恒低、状态机禁止RUN（用于锁相/继电器动作检查）。
 * 0 = 正常运行：20kHz ISR执行完整采样+PLL+单相dq双闭环，可进入RUN
 *     （恢复功率闭环测试前必须确认采样标定/TZ/功率方向，见工程报告§10）。
 */
#ifndef BOARD_PLL_RELAY_TEST_ONLY
#define BOARD_PLL_RELAY_TEST_ONLY          1U
#endif

/* PLL纯软件输入模拟：一个50Hz相位源派生三相，峰值10V，仅限台架模式。 */
#define BOARD_PLL_INPUT_SIMULATION         0U
#define BOARD_PLL_SIM_FREQ_HZ             50.0f
#define BOARD_PLL_SIM_VPEAK_V             10.0f

#if (BOARD_PLL_INPUT_SIMULATION != 0U) && (BOARD_PLL_RELAY_TEST_ONLY == 0U)
#error "PLL input simulation is only allowed in relay-test bench mode"
#endif

/*
 * GPIO21 启停按钮由 CPLD 推挽输出驱动。
 * 实机调试确认: 松开=LOW，按下=HIGH；DSP 内部上拉关闭。
 */
#define BOARD_RUN_BTN_ACTIVE_LEVEL     1U

/*
 * 对称消抖: 连续高/低各 50ms (10ms 调度 × 5 拍) 才迁移 PRESSED/RELEASED。
 * 按下与松开使用完全相同的消抖时长。
 */
#define BOARD_RUN_BTN_DEBOUNCE_MS      50U
#define BOARD_RUN_BTN_DEBOUNCE_TICKS   (BOARD_RUN_BTN_DEBOUNCE_MS / 10U)

/* ============================================================
 * 软起动现场调试默认参数
 *
 * 本区域只定义DSP复位后的默认值。运行时状态机只读取run_supervisor.c
 * 中对应的volatile g_xxx变量，现场临时实验应在CCS Expressions修改g_xxx。
 * 参数基本确定后，再修改这里的*_DEFAULT宏；不需要改状态机代码。
 * ============================================================ */

/*
 * 常规预充模式备用门槛。当前低压直测模式不读取此门槛；如恢复常规
 * 预充流程，需按1000V:2V采样比例和实际母线重新整定该门槛。
 */
#define BOARD_PRECHARGE_DONE_V_DEFAULT           (5.0f)

/*
 * 不控整流预充的最长允许时间，单位ms；默认10000ms。
 * 超时仍未达到门槛时，PWM保持封锁并依次断开GPIO23、GPIO22。
 * 现场临时修改g_precharge_timeout_ms；DSP复位后恢复为本宏。
 */
#define BOARD_PRECHARGE_TIMEOUT_MS_DEFAULT       (10000UL)

/*
 * GPIO23闭合、S4/S5/S6旁路预充电阻后的稳定等待时间，单位ms。
 * 等待期间GPIO22/23保持闭合，但PWM仍然封锁；延时结束不代表立即释放PWM，
 * 还必须同时满足PLL就绪、TZ输入正常和系统无FAULT。
 * 现场临时修改g_bypass_delay_ms；DSP复位后恢复为本宏。
 */
#define BOARD_BYPASS_DELAY_MS_DEFAULT            (500UL)

/*
 * LUT到PLL软切换系数达到该值后，才认为相位源切换已基本完成。
 * 单位：无量纲，范围0~1。现场可在CCS Expressions修改
 * g_pll_ready_alpha_min；DSP复位后恢复为本默认值。
 * 该门槛只用于启动许可，不改变PLL算法、锁相判据或alpha淡化过程。
 */
#define BOARD_PLL_READY_ALPHA_MIN_DEFAULT        (0.999f)

/* ================= CPLD SPI ================= */

/*
 * CPLD SPI 协议验证开关 — 临时调试用。
 * 置 1 时每 100ms 发送固定 ±200‰ 调制 + COMMIT 帧。
 * SignalTap 确认 CPLD 寄存器正确后置 0。
 */
#define BOARD_CPLD_PROTOCOL_TEST    0U

/* ================= PLL 软切换 ================= */

/*
 * 开环 LUT → PLL 同步输出的相位交叉淡化切换。
 * 两模式幅值一致 (200‰)，切换只改变相位源。
 * 相位约定差 90° 在 isr.c 内处理: φPLL = θ + π/2。
 */
#define BOARD_PLL_SW_MOD_PERMILL        200U

/* ---- 锁定判据 (前台 10ms, 连续满足 LOCK_TICKS 拍才切换) ---- */
#define BOARD_PLL_LOCK_FREQ_MIN_HZ      49.5f   /* 第一版 ±0.5Hz, VOFA 看清抖动后再收紧 */
#define BOARD_PLL_LOCK_FREQ_MAX_HZ      50.5f
#define BOARD_PLL_LOCK_VQ_RATIO         0.03f   /* |vq| < 3% × vmag */
#define BOARD_PLL_LOCK_VD_RATIO         0.9f    /* vd > 0.9 × vmag, 防 θ 差 π 反向锁定 */
/*
 * PLL有效电网电压门槛，单位V，作用于Clarke变换后的αβ矢量峰值。
 * 低压直测取3V峰值，按当前Vac比例约34个ADC有效count；上机必须确认
 * 零输入噪声明显低于该门槛。PLL误差已按vmag归一化，锁相动态不依赖幅值。
 * 该宏同时用于PLL内部门控和前台200ms锁定判决，禁止分别设置造成语义不一致。
 */
#define BOARD_PLL_LOCK_VMAG_MIN_V         10.0f

/* ---- 迟滞: 锁上要慢(防抖), 撤出要快 (10ms 调度周期) ---- */
#define BOARD_PLL_LOCK_DEBOUNCE_MS      200U
#define BOARD_PLL_UNLOCK_DEBOUNCE_MS    100U
#define BOARD_PLL_LOCK_TICKS            (BOARD_PLL_LOCK_DEBOUNCE_MS / 10U)
#define BOARD_PLL_UNLOCK_TICKS          (BOARD_PLL_UNLOCK_DEBOUNCE_MS / 10U)

/*
 * 相位交叉淡化时长。200ms 时最大瞬时相位追赶速度
 * = dα/dt × π = (1/0.2) × π ≈ 15.7 rad/s ≈ 2.5 Hz。
 */
#define BOARD_PLL_FADE_MS               200U

/* ================= PLL串口在线调参 ================= */
#define BOARD_PLL_KP_DEFAULT              120.0f
#define BOARD_PLL_KI_DEFAULT             2000.0f
#define BOARD_PLL_FREQ_MIN_HZ_DEFAULT      45.0f
#define BOARD_PLL_FREQ_MAX_HZ_DEFAULT      55.0f
#define BOARD_PLL_FREQ_NOM_HZ_DEFAULT      50.0f
#define BOARD_PLL_VQ_LOCK_RATIO_DEFAULT     0.03f
#define BOARD_PLL_VQ_UNLOCK_RATIO_DEFAULT   0.06f

#define BOARD_PLL_KP_MIN                    0.0f
#define BOARD_PLL_KP_MAX                  500.0f
#define BOARD_PLL_KI_MIN                    0.0f
#define BOARD_PLL_KI_MAX                10000.0f
#define BOARD_PLL_FREQ_MIN_ALLOWED_HZ      40.0f
#define BOARD_PLL_FREQ_MAX_ALLOWED_HZ      60.0f
#define BOARD_PLL_VQ_RATIO_MIN              0.001f
#define BOARD_PLL_VQ_RATIO_MAX              0.50f

/* ================= 单相/三相独立双闭环共用参数 ================= */
#define BOARD_VDC_TARGET_V_DEFAULT           (70.0f)    /* 本次100Vrms空载首测的Vdc_avg默认目标，接近单桥两路均分后的自然电压70.7V。 */
#define BOARD_VDC_RAMP_RATE_VPS_DEFAULT      (10.0f)    /* Vdc参考斜坡速度，单位V/s。 */
#define BOARD_I_LIMIT_A_DEFAULT               (1.50f)    /* 电压外环输出的交流电流峰值上限，单位A；这是控制限幅，不替代硬件过流TZ保护。 */
#define BOARD_M_LIMIT_DEFAULT                 (0.20f)   /* 调制量m绝对值上限；0.20表示限制在±20%。 */
#define BOARD_KP_V_DEFAULT                    (0.02f)   /* 直流电压外环比例增益Kp。 */
#define BOARD_KI_V_DEFAULT                    (2.0f)    /* 直流电压外环积分增益Ki；外环按1kHz执行。 */
#define BOARD_KP_I_DEFAULT                    (6.0f)    /* 交流电流内环比例增益Kp。 */
#define BOARD_KI_I_DEFAULT                 (1200.0f)    /* 交流电流内环积分增益Ki；内环按20kHz执行。 */
#define BOARD_RGRID_OHM_DEFAULT               (0.0f)    /* 网侧等效电阻前馈值，单位Ω；0表示暂不补偿。 */
#define BOARD_POWER_SIGN_DEFAULT              (1.0f)    /* 电流参考方向：+1为当前默认整流方向，-1表示反向。 */

/* ================= 串口在线参数写入上下限 ================= */

/*
 * 以下上下限只约束 PLL_HOST_PROTOCOL（SCI-C RX 协议）的 SET/GET 参数写入，
 * 不参与任何控制算法；控制代码读取的仍是 g_xxx 运行期变量。
 * 越界写入一律拒绝（返回无效帧计数，不修改任何变量）。
 */
#define BOARD_VDC_TARGET_MIN_V            (0.0f)
#define BOARD_VDC_TARGET_MAX_V          (600.0f)
#define BOARD_I_LIMIT_MIN_A               (0.0f)
#define BOARD_I_LIMIT_MAX_A             (100.0f)
#define BOARD_M_LIMIT_MIN                 (0.0f)
#define BOARD_M_LIMIT_MAX                 (0.98f)   /* 与 control_closedloop.c 的 m 钳位上限一致 */
#define BOARD_CURRENT_KP_MIN              (0.0f)
#define BOARD_CURRENT_KP_MAX            (100.0f)
#define BOARD_CURRENT_KI_MIN              (0.0f)
#define BOARD_CURRENT_KI_MAX          (100000.0f)

/* ================= 调试输出 ================= */

/*
 * 串口调试通道选择:
 *   1 = JustFloat 协议 (VOFA+ 观察 PLL/闭环/安全链，Modbus 停用)
 *   0 = Modbus RTU (HR0-11 ADC raw，原模式)
 */
#define BOARD_DEBUG_JUSTFLOAT_ENABLE   1U

/*
 * JustFloat 固定 6 通道帧（6×float + 帧尾 = 28B/帧）。
 * g_jf_lite_mode 运行时切换通道组，不改变帧长：
 *   0 = 实测线电压 Vab/Vbc/Vca + Ia/Ib/Ic
 *   1 = Vdc1..Vdc6
 *   2 = 重构相电压 Va/Vb/Vc + PLL 三相跟随波
 * 1kHz DebugSnapshot 更新上述通道所需采样值。
 * 12 路 ADC 采样、sequencer、PLL、dq、PWM、安全链全部保持原样。
 */
#define JUSTFLOAT_LITE_MODE_LINE       0U   /* Vab/Vbc/Vca + Ia/Ib/Ic */
#define JUSTFLOAT_LITE_MODE_AC         JUSTFLOAT_LITE_MODE_LINE
#define JUSTFLOAT_LITE_MODE_VDC        1U   /* Vdc1..Vdc6 */
#define JUSTFLOAT_LITE_MODE_PLL        2U   /* Va/Vb/Vc + PLL 跟随波 */
#define BOARD_JUSTFLOAT_LITE_MODE_DEFAULT  JUSTFLOAT_LITE_MODE_LINE

#define BOARD_JUSTFLOAT_ENABLE_DEFAULT  1U
#define BOARD_JUSTFLOAT_PERIOD_MS        1U  /* 1ms任务每拍发送：1kHz，50Hz每周期20点 */
#define BOARD_DEBUG_SNAPSHOT_RATE_HZ  1000UL /* 纯观测快照与JustFloat同频，避免占满20kHz ISR */
#define BOARD_DEBUG_SNAPSHOT_DIVIDER  (BOARD_PWM_FREQ_HZ / BOARD_DEBUG_SNAPSHOT_RATE_HZ)

#if (BOARD_JUSTFLOAT_PERIOD_MS < 1U)
#error "BOARD_JUSTFLOAT_PERIOD_MS must be >= 1"
#endif

#if (BOARD_DEBUG_SNAPSHOT_RATE_HZ == 0UL) || \
    ((BOARD_PWM_FREQ_HZ % BOARD_DEBUG_SNAPSHOT_RATE_HZ) != 0UL)
#error "BOARD_DEBUG_SNAPSHOT_RATE_HZ must divide BOARD_PWM_FREQ_HZ exactly"
#endif

#endif
