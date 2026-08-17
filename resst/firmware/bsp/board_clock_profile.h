/* Created by Siquanning */
#ifndef BOARD_CLOCK_PROFILE_H
#define BOARD_CLOCK_PROFILE_H

#include <stdint.h>   /* uint32_t（身份变量声明用） */

/*
 * BOARD_CLOCK_PROFILE_H — 双 Board Clock Profile（编译期选择）
 *
 * 设计原则（用户批准）：
 *   1. Profile 只保存「根参数」（硬件事实 + 功能目标），其余全部编译期推导。
 *   2. 两个 Profile 必须产生相同的「控制系统真实时间行为」：
 *        PWM=20kHz, fast ISR=20kHz, CONTROL_TS=50us, deadtime=1us,
 *        Timer0=100us, slow loop=1kHz, scheduler=1/10/100ms, SCI=576000, SPI≈1MHz
 *   3. 禁止保存互相重复的常量（如同时手写 OSCCLK/PLLCR/SYSCLK/TBPRD/DBRED/BRR）。
 *   4. 非法 Profile 配置直接 #error，无法生成可烧写固件。
 *
 * 选择方式（编译期宏）：
 *   - 默认（未定义）        → BOARD_CLOCK_PROFILE_TARGET_20MHZ（最终正式板）
 *   - 定义 BOARD_CLOCK_PROFILE_DEV_30MHZ → 30MHz 开发板
 *
 * TI 官方依据：
 *   - SPRUI07 Table 1-21/1-22: PLL 使能时 DIVSEL 只能 =0/1(/4) 或 =2(/2)；
 *     DIVSEL=3(/1) 仅 PLL off/bypass 可用。
 *   - SPRS439Q Table 7-4: Flash/OTP wait-state（100/120/150MHz）。
 */

/* ==================================================================
 * 1. Profile 选择
 * ================================================================== */

#if defined(BOARD_CLOCK_PROFILE_DEV_30MHZ)
  #define BOARD_CLOCK_PROFILE_NAME  "DEV_30MHZ"
  #define BOARD_OSCCLK_HZ           30000000UL
  #define BOARD_PLLCR               10U   /* 30MHz×10/2 = 150MHz（150MHz 速度等级 DSP 已确认） */
  #define BOARD_DIVSEL              2U   /* PLL on → /2（DIVSEL=3 非法） */
#else
  #define BOARD_CLOCK_PROFILE_NAME  "TARGET_20MHZ"
  #define BOARD_OSCCLK_HZ           20000000UL
  #define BOARD_PLLCR               10U
  #define BOARD_DIVSEL              2U
#endif

/* ==================================================================
 * 1b. Clock Bring-up 安全模式（BOARD_CLOCK_BRINGUP_ONLY）
 *
 * 仅用于 DEV_30MHZ 的 Clock Bring-up：
 *   - 只验证 30MHz 晶振 → PLL → 150MHz SYSCLK → Timer → Profile 派生值
 *   - 绝对不进行功率输出：GPIO30 恒 LOW，GPIO42/44 恒 OFF，
 *     所有 ePWM 保持 TZ OST Block + AQCSFRC 安全状态。
 * 强制组合：必须与 BOARD_CLOCK_PROFILE_DEV_30MHZ 同时定义，否则 #error。
 * ================================================================== */

#if defined(BOARD_CLOCK_BRINGUP_ONLY)
  #if !defined(BOARD_CLOCK_PROFILE_DEV_30MHZ)
    #error "BOARD_CLOCK_BRINGUP_ONLY is only valid together with BOARD_CLOCK_PROFILE_DEV_30MHZ"
  #endif
  /* 命令行 -DBOARD_CLOCK_BRINGUP_ONLY 已定义（值 1），此处不重定义。 */
#else
  #define BOARD_CLOCK_BRINGUP_ONLY  0U
#endif

/* 固件内 Clock Profile 身份（只读调试信息，定义于 board.c） */
extern const char    g_board_clock_profile_name[];
extern const uint32_t g_board_oscclk_hz;
extern const uint32_t g_board_sysclk_hz;
extern const uint32_t g_board_pwm_freq_hz;
extern const uint32_t g_board_control_ts_us;
extern const uint32_t g_board_clock_bringup;

/* ==================================================================
 * 2. 功能目标（两个 Profile 统一，禁止单独修改某一侧）
 * ================================================================== */

#define BOARD_PWM_FREQ_HZ           20000UL
#define BOARD_CONTROL_TS            (1.0f / (float)BOARD_PWM_FREQ_HZ) /* 50us */
#define BOARD_PWM_COUNT_MODE        2U     /* up-down */
#define BOARD_PWM_DEADTIME_NS       1000UL /* 1.0us */
#define BOARD_TIMER0_PERIOD_US      100UL  /* 100us system tick */
#define BOARD_SCI_BAUD              576000UL
#define BOARD_SPI_TARGET_HZ         1000000UL /* SPI 目标 1MHz */

/* ==================================================================
 * 3. 根时钟推导（PLL 使能，DIVSEL=2 → /2）
 *    SYSCLKOUT = OSCCLK × PLLCR / DIVSEL_div
 * ================================================================== */

#if (BOARD_DIVSEL == 2U)
  #define BOARD_SYSCLK_HZ  ((BOARD_OSCCLK_HZ / 2UL) * BOARD_PLLCR)
#else
  #error "BOARD_DIVSEL must be 2 (/2) for PLL-enabled operation. DIVSEL=3(/1) is only valid when PLL is off/bypassed (SPRUI07 Table 1-22)."
#endif

#define BOARD_HISPCP_DIV             2U   /* HISPCP=0x0001 → /2 */
#define BOARD_LOSPCP_DIV             2U   /* LOSPCP=0x0001 → /2 */
#define BOARD_HSPCLK_HZ              (BOARD_SYSCLK_HZ / BOARD_HISPCP_DIV)
#define BOARD_LSPCLK_HZ              (BOARD_SYSCLK_HZ / BOARD_LOSPCP_DIV)

/* ePWM: CLKDIV=1, HSPCLKDIV=1 → TBCLK = EPWMCLK = SYSCLKOUT */
#define BOARD_EPWM_TBCLK_HZ          BOARD_SYSCLK_HZ

/* ADC 时钟分频（ADCCLKPS、CPS、ACQ_PS）——Profile 派生输入 */
#define BOARD_ADC_ADCCLKPS           3U
#define BOARD_ADC_CPS                0U
#define BOARD_ADC_ACQ_PS             7U
#define BOARD_ADC_CLK_HZ             (BOARD_HSPCLK_HZ / (2UL * (BOARD_ADC_ADCCLKPS + 0UL)))
/* 注: F2833x ADC: ADCCLK = HSPCLK / (ADCCLKPS × 2)（CPS=0 时） */
#if (BOARD_ADC_CPS != 0U)
  #define BOARD_ADC_CLK_HZ_ACTUAL    (BOARD_ADC_CLK_HZ / 2UL)
#else
  #define BOARD_ADC_CLK_HZ_ACTUAL    BOARD_ADC_CLK_HZ
#endif

/* ==================================================================
 * 4. 功能时序 → 寄存器值派生
 * ================================================================== */

/* TBPRD（up-down, 20kHz）: TBPRD = TBCLK / (2 × f_pwm) */
#define BOARD_PWM_TBPRD              (BOARD_EPWM_TBCLK_HZ / (2UL * BOARD_PWM_FREQ_HZ))

/* 死区计数: DBRED/DBFED = TBCLK × deadtime = TBCLK_HZ × DEADTIME_NS / 1e9
 * = (TBCLK_HZ / 1000000UL) × DEADTIME_NS / 1000UL   （避免浮点） */
#define BOARD_PWM_DB_RED             ((BOARD_EPWM_TBCLK_HZ / 1000000UL) * (BOARD_PWM_DEADTIME_NS / 1000UL))
#define BOARD_PWM_DB_FED             BOARD_PWM_DB_RED

/* Timer0 PRD: PRD = SYSCLK × period_us（MHz×us = 计数） */
#define BOARD_TIMER0_PRD             ((BOARD_SYSCLK_HZ / 1000000UL) * BOARD_TIMER0_PERIOD_US)

/* SCI BRR（DrvSci 四舍五入公式一致）: divisor = (LSPCLK + baud×4) / (baud×8); BRR = divisor - 1 */
#define BOARD_SCI_DIVISOR            ((BOARD_LSPCLK_HZ + (BOARD_SCI_BAUD * 4UL)) / (BOARD_SCI_BAUD * 8UL))
#define BOARD_SCI_BRR                (BOARD_SCI_DIVISOR - 1UL)
#define BOARD_SCI_ACTUAL_BAUD        (BOARD_LSPCLK_HZ / (8UL * BOARD_SCI_DIVISOR))

/* SPI BRR: SPICLK = LSPCLK / (BRR+1), BRR>=3。取最接近目标的 BRR */
#define BOARD_SPI_BRR                ((BOARD_LSPCLK_HZ / BOARD_SPI_TARGET_HZ) - 1UL)
#define BOARD_SPI_ACTUAL_SCLK        (BOARD_LSPCLK_HZ / (BOARD_SPI_BRR + 1UL))

/* 控制 ISR 周期 CPU 预算（WCET 上限参考）: SYSCLK / ISR_FREQ */
#define BOARD_FAST_ISR_CYCLE_BUDGET  (BOARD_SYSCLK_HZ / BOARD_PWM_FREQ_HZ)

/* ==================================================================
 * 5. 编译期安全检查（非法配置直接 #error）
 * ================================================================== */

/* SYSCLK ≤ 150 MHz */
#if (BOARD_SYSCLK_HZ > 150000000UL)
  #error "BOARD_SYSCLK_HZ exceeds F28335 150 MHz limit"
#endif

/* 20MHz 晶振禁止尝试 140MHz（不可达） */
#if (BOARD_OSCCLK_HZ == 20000000UL) && (BOARD_PLLCR == 7U) && (BOARD_DIVSEL == 2U)
  /* PLLCR=7,DIVSEL=2 → 70MHz，合法；140MHz 需要 DIVSEL=3 非法。此处仅为文档。 */
#endif

/* PWM 频率必须精确 20kHz（TBPRD 整除校验） */
#if (BOARD_EPWM_TBCLK_HZ % (2UL * BOARD_PWM_FREQ_HZ)) != 0UL
  #error "TBCLK does not divide evenly to 20kHz PWM (up-down)"
#endif

/* TBPRD 合法范围（1..65535） */
#if (BOARD_PWM_TBPRD < 1UL) || (BOARD_PWM_TBPRD > 65535UL)
  #error "BOARD_PWM_TBPRD out of 16-bit range"
#endif

/* 死区计数 10-bit（≤1023） */
#if (BOARD_PWM_DB_RED > 1023UL) || (BOARD_PWM_DB_FED > 1023UL)
  #error "Dead-band count exceeds 10-bit DBRED/DBFED range (max 1023)"
#endif

/* ADC 时钟 ≤ 25 MHz */
#if (BOARD_ADC_CLK_HZ_ACTUAL > 25000000UL)
  #error "ADCCLK exceeds F28335 25 MHz limit"
#endif

/* SCI 波特率误差 ≤ 2% */
#if (BOARD_SCI_BAUD != 0UL)
  #if (BOARD_SCI_ACTUAL_BAUD > BOARD_SCI_BAUD)
    #if (((BOARD_SCI_ACTUAL_BAUD - BOARD_SCI_BAUD) * 100UL) / BOARD_SCI_BAUD) > 2UL
      #error "SCI baud error exceeds 2%"
    #endif
  #else
    #if (((BOARD_SCI_BAUD - BOARD_SCI_ACTUAL_BAUD) * 100UL) / BOARD_SCI_BAUD) > 2UL
      #error "SCI baud error exceeds 2%"
    #endif
  #endif
#endif

/* SPI BRR ≥ 3（F2833x 要求） */
#if (BOARD_SPI_BRR < 3UL)
  #error "SPIBRR must be >= 3"
#endif

/* Timer0 PRD 32-bit 范围内 */
#if (BOARD_TIMER0_PRD == 0UL) || (BOARD_TIMER0_PRD > 0xFFFFFFFFUL)
  #error "Timer0 PRD out of range"
#endif

#endif /* BOARD_CLOCK_PROFILE_H */
