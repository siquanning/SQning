#ifndef CONTROL_CLOSEDLOOP_H
#define CONTROL_CLOSEDLOOP_H

#include <stdint.h>

#define CTRL_TEST_PHASE_A         1U
#define CTRL_TEST_PHASE_B         2U
#define CTRL_TEST_PHASE_C         3U
#define CTRL_TEST_PHASE_DEFAULT   CTRL_TEST_PHASE_A

#define CTRL_RUN_MODE_SINGLE_PHASE  1U
#define CTRL_RUN_MODE_THREE_PHASE   2U
#define CTRL_RUN_MODE_DEFAULT       CTRL_RUN_MODE_SINGLE_PHASE

typedef struct {
    float vac, iac;
    float vdc_avg, vdc_balance;
    float vdc_ref_ramp, vdc_integral;
    float iamp, iref, i_integral;
    float theta_phase, m;
    uint16_t active;
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
                                float theta, const float vdc[6], float ts,
                                int16_t mabc[3]);

#endif
