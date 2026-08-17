#include <math.h>
#include <stdio.h>
#include <string.h>
#include "firmware/control/control_closedloop.h"
#include "firmware/control/control_qsg.h"

static int failures;
#define CHECK(c,m) do { if (!(c)) { printf("FAIL: %s\n",m); failures++; } } while(0)
#define NEAR(a,b,e) (fabsf((a)-(b)) <= (e))

#define W50       (6.283185307f * 50.0f)   /* 2π·50Hz */
#define TS        (50e-6f)
#define PI_HALF   (1.570796327f)
#define TWO_PI_3  (2.094395102f)

static float vac[3], iac[3], vdc[6];
static int16_t mabc[3];

static void defaults(void)
{
    int i;
    g_vdc_target_v=450.0f; g_vdc_ramp_rate_vps=10.0f;
    g_i_limit_a=1.5f; g_m_limit=0.20f;
    g_kp_v=0.02f; g_ki_v=2.0f; g_kp_i=6.0f; g_ki_i=1200.0f;
    g_rgrid_ohm=0.0f; g_power_sign=1.0f;
    g_ctrl_run_mode=CTRL_RUN_MODE_DEFAULT; g_ctrl_test_phase=CTRL_TEST_PHASE_A;
    memset(vac,0,sizeof(vac)); memset(iac,0,sizeof(iac));
    for(i=0;i<6;i++) vdc[i]=400.0f+(float)(i*10);
    ClosedLoop_Init();
}

/* 单相 A 跑 n 拍: iac[0] = amp·cos(θ+phoff)（与 A 相电压同角 θ 比较） */
static void run_single_phase(float amp, float phoff, int n)
{
    int k;
    for (k = 0; k < n; k++) {
        float th = (float)k * W50 * TS;
        vac[0] = 100.0f * cosf(th);
        iac[0] = amp * cosf(th + phoff);
        ClosedLoop_FastStepAll(1U, vac, iac, th, W50, vdc, TS, mabc);
    }
}

static void test_mapping_and_latches(void)
{
    ClosedLoopPhaseMap map;
    defaults();
    CHECK(ClosedLoop_GetPhaseMap(1U,&map) && map.vac_index==0U &&
          map.iac_index==0U && map.vdc_first_index==0U && map.pwm_first_module==1U,
          "A maps Va/Ia/Vdc1-2/ePWM1-2");
    CHECK(ClosedLoop_GetPhaseMap(2U,&map) && map.vac_index==1U &&
          map.iac_index==1U && map.vdc_first_index==2U && map.pwm_first_module==3U,
          "B maps Vb/Ib/Vdc3-4/ePWM3-4");
    CHECK(ClosedLoop_GetPhaseMap(3U,&map) && map.vac_index==2U &&
          map.iac_index==2U && map.vdc_first_index==4U && map.pwm_first_module==5U,
          "C maps Vc/Ic/Vdc5-6/ePWM5-6");
    g_ctrl_test_phase=CTRL_TEST_PHASE_B;
    CHECK(ClosedLoop_LatchRunConfig()==CTRL_RUN_MODE_SINGLE_PHASE,
          "single mode latches");
    g_ctrl_test_phase=CTRL_TEST_PHASE_C; g_ctrl_run_mode=CTRL_RUN_MODE_THREE_PHASE;
    CHECK(ClosedLoop_GetActivePhase()==CTRL_TEST_PHASE_B &&
          ClosedLoop_GetActiveRunMode()==CTRL_RUN_MODE_SINGLE_PHASE,
          "RUN request edits do not hot switch latched config");
    ClosedLoop_ClearActiveConfig(); g_ctrl_run_mode=9U;
    CHECK(ClosedLoop_LatchRunConfig()==0U,"invalid mode is rejected");
}

static void test_three_phase_takeover_phase_and_modulation(void)
{
    defaults(); g_ctrl_run_mode=CTRL_RUN_MODE_THREE_PHASE;
    CHECK(ClosedLoop_LatchRunConfig()==CTRL_RUN_MODE_THREE_PHASE,"three phase latches");
    vac[0]=100.0f; vac[1]=-50.0f; vac[2]=25.0f;
    CHECK(ClosedLoop_FastStepAll(1U,vac,iac,0.0f,W50,vdc,TS,mabc),"three phase fast step valid");
    CHECK(NEAR(g_phase_ctrl[0].vdc_ref_ramp,405.0f,1e-6f) &&
          NEAR(g_phase_ctrl[1].vdc_ref_ramp,425.0f,1e-6f) &&
          NEAR(g_phase_ctrl[2].vdc_ref_ramp,445.0f,1e-6f),
          "each phase takeover captures its own Vdc average");
    CHECK(NEAR(g_phase_ctrl[0].theta_phase,0.0f,1e-6f) &&
          NEAR(g_phase_ctrl[1].theta_phase,-2.094395102f,1e-6f) &&
          NEAR(g_phase_ctrl[2].theta_phase,2.094395102f,1e-6f),
          "phase angles are 0/-120/+120 degrees");
    CHECK(mabc[0]!=0 && mabc[1]!=0 && mabc[2]!=0,
          "three phase mode produces mA mB mC (Vac feedforward)");
}

static void test_independent_dq_integrators(void)
{
    int k;
    defaults(); g_ctrl_run_mode=CTRL_RUN_MODE_THREE_PHASE; ClosedLoop_LatchRunConfig();
    /* 外环积分独立（原有行为） */
    ClosedLoop_FastStepAll(1U,vac,iac,0.0f,W50,vdc,TS,mabc);  /* 先激活三相 */
    g_phase_ctrl[0].vdc_ref_ramp=450.0f;
    g_phase_ctrl[1].vdc_ref_ramp=g_phase_ctrl[1].vdc_avg;
    g_phase_ctrl[2].vdc_ref_ramp=440.0f;
    ClosedLoop_SlowStepAll(1U,0.001f);
    CHECK(g_phase_ctrl[0].vdc_integral != g_phase_ctrl[1].vdc_integral &&
          g_phase_ctrl[1].vdc_integral != g_phase_ctrl[2].vdc_integral,
          "outer integrators are independent");
    /* dq 内环: 三相不同 iamp → 不同 id_ref → 不同 d 积分 */
    for (k = 0; k < 1500; k++) {
        float th = (float)k * W50 * TS;
        g_phase_ctrl[0].iamp=1.0f; g_phase_ctrl[1].iamp=0.5f; g_phase_ctrl[2].iamp=0.2f;
        vac[0]=100.0f*cosf(th);         vac[1]=100.0f*cosf(th-TWO_PI_3); vac[2]=100.0f*cosf(th+TWO_PI_3);
        iac[0]=0.2f*cosf(th);           iac[1]=0.2f*cosf(th-TWO_PI_3);   iac[2]=0.2f*cosf(th+TWO_PI_3);
        ClosedLoop_FastStepAll(1U,vac,iac,th,W50,vdc,TS,mabc);
    }
    CHECK(g_phase_ctrl[0].id_integral != g_phase_ctrl[1].id_integral &&
          g_phase_ctrl[1].id_integral != g_phase_ctrl[2].id_integral,
          "id integrators are independent per phase");
    CHECK(NEAR(g_phase_ctrl[0].id_ref,1.0f,1e-5f) &&
          NEAR(g_phase_ctrl[0].iq_ref,0.0f,1e-6f),
          "id_ref = power_sign*iamp, iq_ref = 0");
    CHECK(fabsf(g_phase_ctrl[0].iq) < 0.2f && fabsf(g_phase_ctrl[2].iq) < 0.2f,
          "in-phase currents keep Iq small");
}

static void test_single_phase_and_invalid_control(void)
{
    defaults(); g_ctrl_test_phase=CTRL_TEST_PHASE_C; ClosedLoop_LatchRunConfig();
    vac[0]=vac[1]=vac[2]=100.0f;
    CHECK(ClosedLoop_FastStepAll(1U,vac,iac,0.0f,W50,vdc,TS,mabc),"single phase step valid");
    CHECK(mabc[0]==0 && mabc[1]==0 && mabc[2]!=0 &&
          !g_phase_ctrl[0].active && !g_phase_ctrl[1].active && g_phase_ctrl[2].active,
          "single phase mode controls only latched target phase");
    vac[1]=sqrtf(-1.0f);
    CHECK(!ClosedLoop_FastStepAll(1U,vac,iac,0.0f,W50,vdc,TS,mabc),
          "non-finite control input is rejected");
}

/* Test1: SOGI 正交性 — 50Hz 稳态 α/β 正交、幅值合理、无 DC 偏置 */
static void test_qsg_orthogonality(void)
{
    QsgSogi q;
    float ph = 0.0f, mean_a = 0.0f, rms_a = 0.0f, rms_b = 0.0f, dot = 0.0f;
    int k;
    Qsg_Init(&q);
    for (k = 0; k < 30 * 400; k++) {           /* 30 周期预热 */
        Qsg_Run(&q, cosf(ph), W50, TS);
        ph += W50 * TS;
    }
    CHECK(fabsf(q.x1 - cosf(ph)) < 0.05f, "alpha in-phase with input");
    CHECK(fabsf(q.x2 - sinf(ph)) < 0.05f, "beta lags alpha by 90deg (sin)");
    for (k = 0; k < 400; k++) {                /* 1 周期统计 */
        Qsg_Run(&q, cosf(ph), W50, TS);
        mean_a += q.x1; rms_a += q.x1*q.x1; rms_b += q.x2*q.x2; dot += q.x1*q.x2;
        ph += W50 * TS;
    }
    mean_a /= 400.0f; rms_a = sqrtf(rms_a/400.0f); rms_b = sqrtf(rms_b/400.0f);
    CHECK(fabsf(mean_a) < 0.02f, "QSG alpha has no DC offset");
    CHECK(fabsf(rms_a - 0.7071f) < 0.02f, "QSG alpha rms ≈ 1/√2");
    CHECK(fabsf(rms_b - 0.7071f) < 0.02f, "QSG beta rms ≈ 1/√2");
    CHECK(fabsf(dot / 400.0f) < 0.02f, "alpha/beta orthogonal (dot≈0)");
}

/* Test2: dq 方向 — 电流与相电压同相 → Id>0, Iq≈0 */
static void test_dq_direction_in_phase(void)
{
    defaults(); ClosedLoop_LatchRunConfig();
    run_single_phase(1.0f, 0.0f, 2000);
    CHECK(g_phase_ctrl[0].id > 0.9f && g_phase_ctrl[0].id < 1.1f,
          "in-phase current -> Id ≈ |I| > 0");
    CHECK(fabsf(g_phase_ctrl[0].iq) < 0.05f, "in-phase current -> Iq ≈ 0");
}

/* Test3: 无功方向 — 滞后90° → Iq<0；超前90° → Iq>0；Id≈0 */
static void test_reactive_direction(void)
{
    defaults(); ClosedLoop_LatchRunConfig();
    run_single_phase(1.0f, -PI_HALF, 2000);
    CHECK(fabsf(g_phase_ctrl[0].id) < 0.05f, "lagging current -> Id ≈ 0");
    CHECK(g_phase_ctrl[0].iq < -0.9f, "lagging current -> Iq < 0 (reactive negative)");

    defaults(); ClosedLoop_LatchRunConfig();
    run_single_phase(1.0f, +PI_HALF, 2000);
    CHECK(fabsf(g_phase_ctrl[0].id) < 0.05f, "leading current -> Id ≈ 0");
    CHECK(g_phase_ctrl[0].iq > 0.9f, "leading current -> Iq > 0 (reactive positive)");
}

/* Test4: A/B/C 映射 — 各自 θ 偏移下同相电流 → Id>0, Iq≈0 */
static void test_abc_mapping(void)
{
    uint16_t ph;
    static const float offs[3] = {0.0f, -TWO_PI_3, TWO_PI_3};
    for (ph = 1U; ph <= 3U; ph++) {
        int idx = (int)(ph - 1U);
        int k;
        defaults(); g_ctrl_test_phase = ph; ClosedLoop_LatchRunConfig();
        for (k = 0; k < 2000; k++) {
            float th = (float)k * W50 * TS;
            vac[idx] = 100.0f * cosf(th + offs[idx]);
            iac[idx] = 1.0f * cosf(th + offs[idx]);
            ClosedLoop_FastStepAll(1U, vac, iac, th, W50, vdc, TS, mabc);
        }
        CHECK(g_phase_ctrl[idx].id > 0.9f && fabsf(g_phase_ctrl[idx].iq) < 0.05f,
              "phase dq mapping: in-phase -> Id>0 Iq≈0");
        CHECK(g_phase_ctrl[idx].active == 1U, "target phase active");
        CHECK(g_phase_ctrl[(idx+1)%3].active == 0U &&
              g_phase_ctrl[(idx+2)%3].active == 0U, "non-target phases inactive");
    }
}

/* Test5: m 限幅 — 大误差 → |m_raw|>limit → m_final == ±g_m_limit */
static void test_m_limit_clamp(void)
{
    int dir;
    for (dir = 0; dir < 2; dir++) {
        int k;
        float max_abs_m = 0.0f;
        defaults(); ClosedLoop_LatchRunConfig();
        for (k = 0; k < 3000; k++) {
            float th = (float)k * W50 * TS;
            vac[0] = 100.0f * cosf(th);
            iac[0] = (dir == 0) ? -2.0f * cosf(th) : 2.0f * cosf(th);
            ClosedLoop_FastStepAll(1U, vac, iac, th, W50, vdc, TS, mabc);
        }
        for (k = 3000; k < 3400; k++) {
            float th = (float)k * W50 * TS;
            vac[0] = 100.0f * cosf(th);
            iac[0] = (dir == 0) ? -2.0f * cosf(th) : 2.0f * cosf(th);
            ClosedLoop_FastStepAll(1U, vac, iac, th, W50, vdc, TS, mabc);
            if (fabsf(g_phase_ctrl[0].m) > max_abs_m)
                max_abs_m = fabsf(g_phase_ctrl[0].m);
        }
        CHECK(max_abs_m <= g_m_limit + 1e-4f,
              dir==0 ? "m_final never exceeds +g_m_limit (pos)" : "m_final never exceeds +g_m_limit (neg)");
        CHECK(max_abs_m >= g_m_limit - 1e-3f,
              dir==0 ? "m_final actually hits +g_m_limit (pos)" : "m_final actually hits +g_m_limit (neg)");
    }
}

/* Test6: disable/enable — 复位 dq 积分/QSG/m，重进无冲击 */
static void test_disable_enable_reset(void)
{
    int k;
    defaults(); ClosedLoop_LatchRunConfig();
    for (k = 0; k < 1000; k++) {
        float th = (float)k * W50 * TS;
        vac[0] = 100.0f * cosf(th);
        iac[0] = 1.0f * cosf(th);
        g_phase_ctrl[0].iamp = 1.0f;
        ClosedLoop_FastStepAll(1U, vac, iac, th, W50, vdc, TS, mabc);
    }
    CHECK(g_phase_ctrl[0].id_integral != 0.0f ||
          g_phase_ctrl[0].iq_integral != 0.0f,
          "integrators accumulate while enabled");
    ClosedLoop_FastStepAll(0U, vac, iac, 0.0f, W50, vdc, TS, mabc);
    CHECK(g_phase_ctrl[0].id_integral == 0.0f && g_phase_ctrl[0].iq_integral == 0.0f &&
          g_phase_ctrl[0].m == 0.0f && g_phase_ctrl[0].m_raw == 0.0f &&
          g_phase_ctrl[0].qsg.x1 == 0.0f && g_phase_ctrl[0].qsg.x2 == 0.0f &&
          g_phase_ctrl[0].active == 0U,
          "disable resets dq integrators/QSG/m/active");
    ClosedLoop_FastStepAll(1U, vac, iac, 0.0f, W50, vdc, TS, mabc);
    CHECK(g_phase_ctrl[0].active == 1U, "re-enable activates phase");
    CHECK(fabsf(g_phase_ctrl[0].id_integral) < 1e-2f &&
          fabsf(g_phase_ctrl[0].iq_integral) < 1e-2f,
          "re-entry starts from (near) zero integrators, no inherited windup");
    CHECK(fabsf(g_phase_ctrl[0].m) < g_m_limit,
          "no inherited integral -> m within limit (no bump)");
}

int main(void)
{
    printf("=== Single-phase dq Closed-loop Tests ===\n");
    test_mapping_and_latches();
    test_three_phase_takeover_phase_and_modulation();
    test_independent_dq_integrators();
    test_single_phase_and_invalid_control();
    test_qsg_orthogonality();
    test_dq_direction_in_phase();
    test_reactive_direction();
    test_abc_mapping();
    test_m_limit_clamp();
    test_disable_enable_reset();
    printf("=== %s ===\n",failures?"SOME TESTS FAILED":"ALL TESTS PASSED");
    return failures?1:0;
}
