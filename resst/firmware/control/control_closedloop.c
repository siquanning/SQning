#include <math.h>
#include "firmware/bsp/board_config.h"
#include "firmware/control/control_closedloop.h"

volatile float g_vdc_target_v = BOARD_VDC_TARGET_V_DEFAULT;
volatile float g_vdc_ramp_rate_vps = BOARD_VDC_RAMP_RATE_VPS_DEFAULT;
volatile float g_i_limit_a = BOARD_I_LIMIT_A_DEFAULT;
volatile float g_m_limit = BOARD_M_LIMIT_DEFAULT;
volatile float g_kp_v = BOARD_KP_V_DEFAULT;
volatile float g_ki_v = BOARD_KI_V_DEFAULT;
volatile float g_kp_i = BOARD_KP_I_DEFAULT;
volatile float g_ki_i = BOARD_KI_I_DEFAULT;
volatile float g_rgrid_ohm = BOARD_RGRID_OHM_DEFAULT;
volatile float g_power_sign = BOARD_POWER_SIGN_DEFAULT;
volatile uint16_t g_ctrl_test_phase = CTRL_TEST_PHASE_DEFAULT;
volatile uint16_t g_ctrl_run_mode = CTRL_RUN_MODE_DEFAULT;
volatile PhaseClosedLoopState g_phase_ctrl[3];

static volatile uint16_t s_ctrl_active_mode;
static volatile uint16_t s_ctrl_active_phase;

static float clampf_local(float x, float lo, float hi)
{
    return (x < lo) ? lo : ((x > hi) ? hi : x);
}

static uint16_t finite_local(float x)
{
    return ((x == x) && (x <= 3.402823466e+38F) &&
            (x >= -3.402823466e+38F)) ? 1U : 0U;
}

uint16_t ClosedLoop_IsValidTestPhase(uint16_t phase)
{
    return ((phase >= CTRL_TEST_PHASE_A) &&
            (phase <= CTRL_TEST_PHASE_C)) ? 1U : 0U;
}

uint16_t ClosedLoop_IsValidRunMode(uint16_t mode)
{
    return ((mode == CTRL_RUN_MODE_SINGLE_PHASE) ||
            (mode == CTRL_RUN_MODE_THREE_PHASE)) ? 1U : 0U;
}

uint16_t ClosedLoop_GetPhaseMap(uint16_t phase, ClosedLoopPhaseMap *map)
{
    static const float offsets[3] = {0.0f, -2.094395102f, 2.094395102f};
    uint16_t index;
    if ((map == ((ClosedLoopPhaseMap *)0)) ||
        (ClosedLoop_IsValidTestPhase(phase) == 0U)) return 0U;
    index = phase - CTRL_TEST_PHASE_A;
    map->phase_index = index;
    map->vac_index = index;
    map->iac_index = index;
    map->vdc_first_index = index * 2U;
    map->pwm_first_module = index * 2U + 1U;
    map->theta_offset_rad = offsets[index];
    return 1U;
}

uint16_t ClosedLoop_LatchRunConfig(void)
{
    uint16_t mode = g_ctrl_run_mode;
    uint16_t phase = g_ctrl_test_phase;
    if ((ClosedLoop_IsValidRunMode(mode) == 0U) ||
        ((mode == CTRL_RUN_MODE_SINGLE_PHASE) &&
         (ClosedLoop_IsValidTestPhase(phase) == 0U))) {
        s_ctrl_active_mode = 0U;
        s_ctrl_active_phase = 0U;
        return 0U;
    }
    s_ctrl_active_mode = mode;
    s_ctrl_active_phase = (mode == CTRL_RUN_MODE_SINGLE_PHASE) ? phase : 0U;
    return mode;
}

uint16_t ClosedLoop_GetActiveRunMode(void) { return s_ctrl_active_mode; }
uint16_t ClosedLoop_GetActivePhase(void) { return s_ctrl_active_phase; }

void ClosedLoop_ClearActiveConfig(void)
{
    s_ctrl_active_mode = 0U;
    s_ctrl_active_phase = 0U;
}

static void reset_phase(volatile PhaseClosedLoopState *s)
{
    s->vdc_integral = 0.0f;
    s->i_integral = 0.0f;
    s->iamp = 0.0f;
    s->iref = 0.0f;
    s->m = 0.0f;
    s->active = 0U;
}

void ClosedLoop_Init(void)
{
    uint16_t i;
    for (i = 0U; i < 3U; i++) {
        g_phase_ctrl[i].vac = 0.0f;
        g_phase_ctrl[i].iac = 0.0f;
        g_phase_ctrl[i].vdc_avg = 0.0f;
        g_phase_ctrl[i].vdc_balance = 0.0f;
        g_phase_ctrl[i].vdc_ref_ramp = 0.0f;
        g_phase_ctrl[i].theta_phase = 0.0f;
        reset_phase(&g_phase_ctrl[i]);
    }
    ClosedLoop_ClearActiveConfig();
}

static uint16_t phase_enabled(uint16_t index)
{
    if (s_ctrl_active_mode == CTRL_RUN_MODE_THREE_PHASE) return 1U;
    if (s_ctrl_active_mode == CTRL_RUN_MODE_SINGLE_PHASE)
        return (s_ctrl_active_phase == index + CTRL_TEST_PHASE_A) ? 1U : 0U;
    return 0U;
}

void ClosedLoop_SlowStepAll(uint16_t enable, float dt_s)
{
    uint16_t i;
    float ramp_step = ((g_vdc_ramp_rate_vps > 0.0f) ? g_vdc_ramp_rate_vps : 0.0f)
                    * ((dt_s > 0.0f) ? dt_s : 0.0f);
    float i_limit = (g_i_limit_a > 0.0f) ? g_i_limit_a : 0.0f;
    for (i = 0U; i < 3U; i++) {
        volatile PhaseClosedLoopState *s = &g_phase_ctrl[i];
        float ev, integral_candidate, output_candidate;
        if ((enable == 0U) || (phase_enabled(i) == 0U) || (s->active == 0U)) {
            if ((enable == 0U) || (phase_enabled(i) == 0U)) reset_phase(s);
            continue;
        }
        if (s->vdc_ref_ramp < g_vdc_target_v) {
            s->vdc_ref_ramp += ramp_step;
            if (s->vdc_ref_ramp > g_vdc_target_v) s->vdc_ref_ramp = g_vdc_target_v;
        } else if (s->vdc_ref_ramp > g_vdc_target_v) {
            s->vdc_ref_ramp -= ramp_step;
            if (s->vdc_ref_ramp < g_vdc_target_v) s->vdc_ref_ramp = g_vdc_target_v;
        }
        ev = s->vdc_ref_ramp - s->vdc_avg;
        integral_candidate = s->vdc_integral + g_ki_v * ev * dt_s;
        output_candidate = g_kp_v * ev + integral_candidate;
        if (!((output_candidate > i_limit && ev > 0.0f) ||
              (output_candidate < 0.0f && ev < 0.0f)))
            s->vdc_integral = integral_candidate;
        s->iamp = clampf_local(g_kp_v * ev + s->vdc_integral, 0.0f, i_limit);
    }
}

uint16_t ClosedLoop_FastStepAll(uint16_t enable,
                                const float vac[3], const float iac[3],
                                float theta, const float vdc[6], float ts,
                                int16_t mabc[3])
{
    static const float offsets[3] = {0.0f, -2.094395102f, 2.094395102f};
    uint16_t i;
    float m_limit = clampf_local(g_m_limit, 0.0f, 0.98f);
    if ((vac == ((const float *)0)) || (iac == ((const float *)0)) ||
        (vdc == ((const float *)0)) || (mabc == ((int16_t *)0)) ||
        (finite_local(theta) == 0U) || (finite_local(ts) == 0U)) return 0U;
    for (i = 0U; i < 3U; i++) {
        volatile PhaseClosedLoopState *s = &g_phase_ctrl[i];
        float vdc_sum = vdc[2U*i] + vdc[2U*i+1U];
        float ierr, integral_candidate, vctrl, m_candidate;
        s->vac = vac[i]; s->iac = iac[i];
        s->vdc_avg = 0.5f * vdc_sum;
        s->vdc_balance = (vdc[2U*i] >= vdc[2U*i+1U])
                       ? vdc[2U*i] - vdc[2U*i+1U]
                       : vdc[2U*i+1U] - vdc[2U*i];
        s->theta_phase = theta + offsets[i];
        mabc[i] = 0;
        if ((finite_local(s->vac) == 0U) || (finite_local(s->iac) == 0U) ||
            (finite_local(s->vdc_avg) == 0U) || (finite_local(vdc_sum) == 0U))
            return 0U;
        if ((enable == 0U) || (phase_enabled(i) == 0U)) { reset_phase(s); continue; }
        if (s->active == 0U) {
            s->vdc_ref_ramp = s->vdc_avg;
            s->vdc_integral = 0.0f;
            s->i_integral = 0.0f;
            s->iamp = 0.0f;
            s->iref = 0.0f;
            s->m = 0.0f;
            s->active = 1U;
        }
        s->iref = g_power_sign * s->iamp * cosf(s->theta_phase);
        ierr = s->iref - s->iac;
        integral_candidate = s->i_integral + g_ki_i * ierr * ts;
        vctrl = g_kp_i * ierr + integral_candidate;
        if (vdc_sum > 1.0f) {
            m_candidate = (s->vac - vctrl + g_rgrid_ohm * s->iac) / vdc_sum;
            if (!((m_candidate > m_limit && ierr < 0.0f) ||
                  (m_candidate < -m_limit && ierr > 0.0f)))
                s->i_integral = integral_candidate;
            vctrl = g_kp_i * ierr + s->i_integral;
            s->m = clampf_local((s->vac - vctrl + g_rgrid_ohm*s->iac)/vdc_sum,
                                -m_limit, m_limit);
        } else {
            s->i_integral = 0.0f;
            s->m = 0.0f;
        }
        if ((finite_local(s->iref) == 0U) || (finite_local(s->i_integral) == 0U) ||
            (finite_local(s->m) == 0U)) return 0U;
        mabc[i] = (int16_t)(s->m * 1000.0f);
    }
    return 1U;
}
