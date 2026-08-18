/* Created by Siquanning */
#ifndef CONTROL_CLOSEDLOOP_H
#define CONTROL_CLOSEDLOOP_H

#include <stdint.h>
#include "firmware/control/control_qsg.h"

#define CTRL_TEST_PHASE_A         1U
#define CTRL_TEST_PHASE_B         2U
#define CTRL_TEST_PHASE_C         3U
#define CTRL_TEST_PHASE_DEFAULT   CTRL_TEST_PHASE_A

#define CTRL_RUN_MODE_SINGLE_PHASE  1U
#define CTRL_RUN_MODE_THREE_PHASE   2U
#define CTRL_RUN_MODE_DEFAULT       CTRL_RUN_MODE_SINGLE_PHASE

/*
 * 单相 dq 电流内环状态（每相独立一份；一次 RUN 只激活一相）。
 *
 * Vdc 外环输出 iamp = 有功电流幅值参考 id_ref 幅值；
 * dq 内环: Iac → SOGI → Iα/Iβ → Park(θp) → Id/Iq → 双 PI → Vd_ctrl/Vq_ctrl
 *        → inverse Park → Vctrl_α → m_raw（Vac 前馈）→ ±m_limit → m_final。
 */
typedef struct {
    float vac, iac;              /* 当前相 Vac（重构相电压）/ Iac（控制输入） */
    float vdc_avg, vdc_balance;  /* 直流母线均值 / 不平衡度 */
    float vdc_ref_ramp, vdc_integral;  /* Vdc 外环参考斜坡 / 积分（外环保持原样） */
    float iamp;                  /* 外环输出：电流幅值参考（= id_ref 幅值） */
    float theta_phase;           /* 当前相角 θ + 相偏移（A:0/B:-120°/C:+120°） */
    uint16_t active;             /* 1=本相控制激活 */

    /* ---- dq 电流内环（新增，替换原单相瞬时 PI） ---- */
    QsgSogi qsg;                 /* 单相正交轴 SOGI（每相独立） */
    float i_alpha, i_beta;       /* SOGI 输出（β 滞后 α 90°） */
    float id, iq;                /* Park 后电流 dq */
    float id_ref, iq_ref;        /* dq 参考（iq_ref 固定 0） */
    float id_err, iq_err;        /* dq 误差 */
    float id_integral, iq_integral;  /* dq 独立积分器 */
    float vd_ctrl, vq_ctrl;      /* dq PI 输出（控制修正量） */
    float m_raw;                 /* 钳位前 m（Vac 前馈 − Vctrl_α + Rgrid·Iac）/Vdc_sum */
    float m;                     /* 钳位后 m（±g_m_limit） */
} PhaseClosedLoopState;

typedef struct {
    uint16_t phase_index;
    uint16_t vac_index;
    uint16_t iac_index;
    uint16_t vdc_first_index;
    uint16_t pwm_first_module;
    float theta_offset_rad;
} ClosedLoopPhaseMap;

/* 运行时可调参数（CCS Expressions）。 */
extern volatile float g_vdc_target_v;
extern volatile float g_vdc_ramp_rate_vps;
extern volatile float g_i_limit_a;
extern volatile float g_m_limit;
extern volatile float g_kp_v;
extern volatile float g_ki_v;
extern volatile float g_kp_i;
extern volatile float g_ki_i;
extern volatile float g_rgrid_ohm;
extern volatile float g_power_sign;
extern volatile uint16_t g_ctrl_test_phase;
extern volatile uint16_t g_ctrl_run_mode;
extern volatile PhaseClosedLoopState g_phase_ctrl[3];

uint16_t ClosedLoop_IsValidTestPhase(uint16_t phase);
uint16_t ClosedLoop_IsValidRunMode(uint16_t mode);
uint16_t ClosedLoop_GetPhaseMap(uint16_t phase, ClosedLoopPhaseMap *map);
uint16_t ClosedLoop_LatchRunConfig(void);
uint16_t ClosedLoop_GetActiveRunMode(void);
uint16_t ClosedLoop_GetActivePhase(void);
void ClosedLoop_ClearActiveConfig(void);

void ClosedLoop_Init(void);
void ClosedLoop_SlowStepAll(uint16_t enable, float dt_s);
uint16_t ClosedLoop_FastStepAll(uint16_t enable,
                                const float vac[3], const float iac[3],
                                float theta, float omega,
                                const float vdc[6], float ts,
                                int16_t mabc[3]);

#endif
