#include <math.h>
#include <stdio.h>
#include <string.h>
#include "firmware/control/control_closedloop.h"

static int failures;
#define CHECK(c,m) do { if (!(c)) { printf("FAIL: %s\n",m); failures++; } } while(0)
#define NEAR(a,b,e) (fabsf((a)-(b)) <= (e))

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
    CHECK(ClosedLoop_FastStepAll(1U,vac,iac,0.0f,vdc,50e-6f,mabc),"three phase fast step valid");
    CHECK(NEAR(g_phase_ctrl[0].vdc_ref_ramp,405.0f,1e-6f) &&
          NEAR(g_phase_ctrl[1].vdc_ref_ramp,425.0f,1e-6f) &&
          NEAR(g_phase_ctrl[2].vdc_ref_ramp,445.0f,1e-6f),
          "each phase takeover captures its own Vdc average");
    CHECK(NEAR(g_phase_ctrl[0].theta_phase,0.0f,1e-6f) &&
          NEAR(g_phase_ctrl[1].theta_phase,-2.094395102f,1e-6f) &&
          NEAR(g_phase_ctrl[2].theta_phase,2.094395102f,1e-6f),
          "phase angles are 0/-120/+120 degrees");
    CHECK(mabc[0]!=0 && mabc[1]!=0 && mabc[2]!=0,
          "three phase mode produces mA mB mC simultaneously");
}

static void test_independent_integrators_and_cos_refs(void)
{
    defaults(); g_ctrl_run_mode=CTRL_RUN_MODE_THREE_PHASE; ClosedLoop_LatchRunConfig();
    ClosedLoop_FastStepAll(1U,vac,iac,0.0f,vdc,50e-6f,mabc);
    g_phase_ctrl[0].vdc_ref_ramp=450.0f;
    g_phase_ctrl[1].vdc_ref_ramp=g_phase_ctrl[1].vdc_avg;
    g_phase_ctrl[2].vdc_ref_ramp=440.0f;
    ClosedLoop_SlowStepAll(1U,0.001f);
    CHECK(g_phase_ctrl[0].vdc_integral != g_phase_ctrl[1].vdc_integral &&
          g_phase_ctrl[1].vdc_integral != g_phase_ctrl[2].vdc_integral,
          "outer integrators are independent");
    g_phase_ctrl[0].iamp=g_phase_ctrl[1].iamp=g_phase_ctrl[2].iamp=1.0f;
    iac[0]=0.0f; iac[1]=0.2f; iac[2]=-0.3f;
    ClosedLoop_FastStepAll(1U,vac,iac,0.0f,vdc,50e-6f,mabc);
    CHECK(g_phase_ctrl[0].i_integral != g_phase_ctrl[1].i_integral &&
          g_phase_ctrl[1].i_integral != g_phase_ctrl[2].i_integral,
          "inner integrators are independent");
    CHECK(NEAR(g_phase_ctrl[0].iref,1.0f,1e-5f) &&
          NEAR(g_phase_ctrl[1].iref,-0.5f,1e-5f) &&
          NEAR(g_phase_ctrl[2].iref,-0.5f,1e-5f),
          "cos(theta_phase) creates 120-degree three-phase references");
}

static void test_single_phase_and_invalid_control(void)
{
    defaults(); g_ctrl_test_phase=CTRL_TEST_PHASE_C; ClosedLoop_LatchRunConfig();
    vac[0]=vac[1]=vac[2]=100.0f;
    ClosedLoop_FastStepAll(1U,vac,iac,0.0f,vdc,50e-6f,mabc);
    CHECK(mabc[0]==0 && mabc[1]==0 && mabc[2]!=0 &&
          !g_phase_ctrl[0].active && !g_phase_ctrl[1].active && g_phase_ctrl[2].active,
          "single phase mode controls only latched target phase");
    vac[1]=sqrtf(-1.0f);
    CHECK(!ClosedLoop_FastStepAll(1U,vac,iac,0.0f,vdc,50e-6f,mabc),
          "non-finite control input is rejected");
}

int main(void)
{
    printf("=== Three-phase Independent Closed-loop Tests ===\n");
    test_mapping_and_latches();
    test_three_phase_takeover_phase_and_modulation();
    test_independent_integrators_and_cos_refs();
    test_single_phase_and_invalid_control();
    printf("=== %s ===\n",failures?"SOME TESTS FAILED":"ALL TESTS PASSED");
    return failures?1:0;
}
