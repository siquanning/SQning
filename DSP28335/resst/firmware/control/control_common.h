#ifndef CONTROL_COMMON_H
#define CONTROL_COMMON_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 调制参数: m ∈ [-980, +980] per-mill, 对应占空比 1%–99% */
#define MOD_M_PERMILL_MIN   (-980)
#define MOD_M_PERMILL_MAX   ( 980)
#define MOD_DUTY_PERMILL_MIN   10U
#define MOD_DUTY_PERMILL_MAX   990U

/*
 * Half-bridge command for one ePWM module.
 * A = upper switch, B = lower switch, strictly complementary.
 *
 *   force_high=0 → normal complementary PWM via AQCTLA/AQCTLB + CMPA
 *   force_high=1 → A=HIGH, B=LOW (AQCSFRC=0x0006)
 *
 * Only CMPA carries modulation; CMPB is not part of the H1 chain.
 */
typedef struct
{
    uint16_t cmp;
    uint16_t force_high;
} HalfBridgeCommand;

/*
 * Per-phase PWM command: left + right half-bridge for one H-bridge phase.
 *
 *   left  → ePWM(2N-1): upper=A, lower=B
 *   right → ePWM(2N):   upper=A, lower=B
 */
typedef struct
{
    HalfBridgeCommand left;
    HalfBridgeCommand right;
} PhasePwmCommand;

/* Control_FastStep 输入 — ISR 从 ADC 采样填充 */
typedef struct
{
    uint16_t adc_raw[2];
    uint16_t vbus;
    uint16_t iload;
    uint32_t reserved;
} ControlInput;

/* Control_FastStep 输出 — ISR 消费, 三通道独立 */
typedef struct
{
    uint16_t cmpa[3];
    uint16_t cmpb[3];
    uint16_t force_a[3];    /* 1 = leg A force HIGH, 0 = normal PWM */
    uint16_t force_b[3];    /* 1 = leg B force HIGH, 0 = normal PWM */
    uint16_t valid;
    uint16_t fault_code;
    uint32_t fault_asserted;
} ControlOutput;

/* 控制上下文: 三通道独立调制度 + 内部状态
 * ISR 读取 .active 字段, 绝不直接写 .pending */
typedef struct
{
    int16_t  m_permill[3];       /* m ∈ [-980, +980] per-mill, 每通道独立 */
    uint16_t control_mode;
    uint16_t tbprd;
    uint16_t adc_safe_min;
    uint16_t adc_safe_max;
    uint16_t fault_thresh_adc_stuck;
    uint16_t step_count;
    uint16_t stuck_ctr_high;
    uint16_t stuck_ctr_low;
    uint32_t acc_error;
    uint16_t reserved[4];
} ControlContext;

/* 有效控制模式 */
#define CONTROL_MODE_PASSTHROUGH  0U

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_COMMON_H */
