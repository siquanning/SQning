#ifdef __TMS320C28XX__
static int _host_test_placeholder_run_supervisor;
#else

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "firmware/app/run_supervisor.h"
#include "firmware/app/state_machine.h"
#include "firmware/bsp/board_config.h"
#include "firmware/services/measurement.h"
#include "firmware/control/control_closedloop.h"

MeasurementSample g_measurement;
volatile uint16_t g_pll_switch_req = 0U;
volatile float g_switch_alpha = 0.0f;
volatile float g_switch_phase_err_deg = 0.0f;
volatile uint16_t g_ctrl_test_phase = CTRL_TEST_PHASE_DEFAULT;
volatile uint16_t g_ctrl_run_mode = CTRL_RUN_MODE_DEFAULT;
static uint16_t g_active_phase;
static uint16_t g_active_mode;

enum { ACT_BLOCK = 1, ACT_BYPASS_OFF, ACT_GRID_OFF, ACT_LED_OFF,
       ACT_RELEASE, ACT_GRID_ON, ACT_BYPASS_ON };
static int g_trace[32], g_trace_count;
static int g_pwm_blocked, g_release_calls, g_released_phase, g_led, g_grid, g_bypass, g_trip_clear;
static int g_failures;

static void trace(int action) { if (g_trace_count < 32) g_trace[g_trace_count++] = action; }
void PWM_BlockOutput(void) { g_pwm_blocked = 1; trace(ACT_BLOCK); }
void PWM_ReleaseOutput(void) { g_pwm_blocked = 0; g_release_calls++; }
uint16_t PWM_ReleaseSelectedPhase(uint16_t phase) { g_pwm_blocked = 0; g_release_calls++; g_released_phase = phase; trace(ACT_RELEASE); return 1U; }
uint16_t PWM_ReleaseThreePhase(void) { g_pwm_blocked = 0; g_release_calls++; g_released_phase = 0; trace(ACT_RELEASE); return 1U; }
uint16_t PWM_AreTripInputsClear(void) { return g_trip_clear ? 1U : 0U; }
uint16_t ClosedLoop_IsValidTestPhase(uint16_t p) { return (p >= 1U && p <= 3U) ? 1U : 0U; }
uint16_t ClosedLoop_IsValidRunMode(uint16_t m) { return (m == 1U || m == 2U) ? 1U : 0U; }
uint16_t ClosedLoop_LatchRunConfig(void) { if (!ClosedLoop_IsValidRunMode(g_ctrl_run_mode) || (g_ctrl_run_mode == 1U && !ClosedLoop_IsValidTestPhase(g_ctrl_test_phase))) { g_active_mode=0U; g_active_phase=0U; return 0U; } g_active_mode=g_ctrl_run_mode; g_active_phase=(g_active_mode==1U)?g_ctrl_test_phase:0U; return g_active_mode; }
uint16_t ClosedLoop_GetActiveRunMode(void) { return g_active_mode; }
uint16_t ClosedLoop_GetActivePhase(void) { return g_active_phase; }
void ClosedLoop_ClearActiveConfig(void) { g_active_mode=0U; g_active_phase=0U; }
void DrvGpio_WriteRunState(uint16_t level) { g_led = level ? 1 : 0; if (!level) trace(ACT_LED_OFF); }
void DrvGpio_WriteFaultGate(uint16_t level) { (void)level; }
void DrvGpio_WriteGridSwitch(uint16_t on) { g_grid = on ? 1 : 0; trace(on ? ACT_GRID_ON : ACT_GRID_OFF); }
void DrvGpio_WritePrechargeBypass(uint16_t on) { g_bypass = on ? 1 : 0; trace(on ? ACT_BYPASS_ON : ACT_BYPASS_OFF); }
void DrvInterrupt_DisableGlobal(void) { }
void DrvInterrupt_RestoreGlobal(void) { }

#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); g_failures++; } } while (0)

static void sm_to_standby(StateMachine *sm)
{
    StateMachine_Init(sm, 0UL);
    StateMachine_Service(sm, 0UL, 0UL);
    StateMachine_Service(sm, 0UL, 0UL);
}

static void reset_fixture(RunSupervisor *rs, StateMachine *sm)
{
    unsigned i;
    memset(&g_measurement, 0, sizeof(g_measurement));
    g_pwm_blocked = 1; g_release_calls = 0; g_released_phase = 0; g_led = 0;
    /* Start dirty to prove RunSupervisor_Init actively opens both relays. */
    g_grid = 1; g_bypass = 1; g_trip_clear = 1;
    g_pll_switch_req = 0U; g_switch_alpha = 0.0f;
    g_ctrl_test_phase = CTRL_TEST_PHASE_DEFAULT; g_ctrl_run_mode = CTRL_RUN_MODE_DEFAULT;
    g_active_phase = 0U; g_active_mode = 0U;
    g_trace_count = 0;
    for (i = 0; i < 32; i++) g_trace[i] = 0;
    RunSupervisor_Init(rs);
    CHECK(!g_grid && !g_bypass,
          "RunSupervisor_Init actively opens GPIO42/GPIO44 relays");
    /*
     * RunSupervisor_Init会加载板级DEFAULT。主机测试随后覆盖为小量程夹具值，
     * 使状态逻辑测试不依赖现场预充电压默认值，避免硬件标定变化造成假失败。
     */
    g_precharge_done_v = 1.0f;
    g_precharge_timeout_ms = 10000UL;
    g_bypass_delay_ms = 500UL;
    g_pll_ready_alpha_min = 0.999f;
    sm_to_standby(sm);
    RunSupervisor_Service(rs, sm, 0U, 0UL); /* clear power-up inhibit */
    g_trace_count = 0;
}

/* 启动到 BYPASS_WAIT 的夹具（宏相关）：直测=立即；预充=过门槛后 */
static void begin_run_wait(RunSupervisor *rs, StateMachine *sm, uint32_t now)
{
    RunSupervisor_Service(rs, sm, 1U, now);
    CHECK(sm->state == SYSTEM_STATE_STANDBY, "start keeps STANDBY");
#if (BOARD_LOW_VOLTAGE_DIRECT_TEST != 0U)
    CHECK(rs->seq_state == START_SEQ_BYPASS_WAIT, "direct mode enters PLL/TZ wait");
    CHECK(g_pwm_blocked && g_grid && g_bypass && !g_led,
          "direct wait: GPIO42/44 closed, PWM blocked");
#else
    CHECK(rs->seq_state == START_SEQ_PRECHARGE, "START enters PRECHARGE");
    CHECK(g_pwm_blocked && g_grid && !g_bypass && !g_led,
          "precharge: grid on, bypass off, PWM blocked");
    g_measurement.vdc_v[0]=100.0f; g_measurement.vdc_v[1]=100.0f;
    g_measurement.vdc_v[2]=100.0f; g_measurement.vdc_v[3]=100.0f;
    g_measurement.vdc_v[4]=100.0f; g_measurement.vdc_v[5]=100.0f;
    RunSupervisor_Service1ms(rs, sm, now);
    CHECK(rs->seq_state == START_SEQ_BYPASS_WAIT, "precharge done -> BYPASS_WAIT");
    CHECK(g_grid && g_bypass && g_pwm_blocked, "bypass on while PWM remains blocked");
#endif
}

/* RUN 尝试时间点：直测无旁路延时(可立即)，预充需等 500ms=5000 tick */
#define RUN_AT_TICK  ((BOARD_LOW_VOLTAGE_DIRECT_TEST != 0U) ? 120UL : 5100UL)

static void check_stop_order(RunSupervisor *rs, StateMachine *sm, uint32_t now,
                             const char *phase)
{
    g_trace_count = 0;
    RunSupervisor_Service(rs, sm, 0U, now);
    CHECK(g_trace_count >= 4, phase);
    CHECK(g_trace[0] == ACT_BLOCK, "STOP first action PWM Block");
    CHECK(g_trace[1] == ACT_BYPASS_OFF, "STOP second action GPIO23 off");
    CHECK(g_trace[2] == ACT_GRID_OFF, "STOP third action GPIO22 off");
    CHECK(sm->state == SYSTEM_STATE_STANDBY, "STOP requests STANDBY after switches");
    CHECK(g_trace[3] == ACT_LED_OFF, "STOP LED off after switch opening");
    CHECK(!g_grid && !g_bypass && g_pwm_blocked && !g_led, "STOP safe outputs");
    CHECK(rs->seq_state == START_SEQ_IDLE, "STOP resets sequence");
}

/* Wrap RequestStandby only through observable state; trace is supplied by state result. */
static void test_powerup_and_direct_wait(void)
{
    RunSupervisor rs; StateMachine sm;
    RunSupervisor_Init(&rs); sm_to_standby(&sm);
    RunSupervisor_Service(&rs, &sm, 1U, 0UL);
    CHECK(sm.state == SYSTEM_STATE_STANDBY && rs.seq_state == START_SEQ_IDLE,
          "power-up GPIO21 high cannot auto-start");
    RunSupervisor_Service(&rs, &sm, 0U, 100UL);
    RunSupervisor_Service(&rs, &sm, 1U, 200UL);
#if (BOARD_LOW_VOLTAGE_DIRECT_TEST != 0U)
    CHECK(rs.seq_state == START_SEQ_BYPASS_WAIT && g_grid && g_bypass,
          "0 then 1 closes GPIO42/44 before waiting for PLL");
#else
    CHECK(rs.seq_state == START_SEQ_PRECHARGE && g_grid && !g_bypass,
          "0 then 1 enters PRECHARGE with grid on, bypass off");
#endif
}

static void test_run_gates(void)
{
    RunSupervisor rs; StateMachine sm;
    reset_fixture(&rs, &sm); begin_run_wait(&rs, &sm, 100UL);

    g_pll_switch_req = 1U; g_switch_alpha = 0.9f;
    RunSupervisor_Service(&rs, &sm, 1U, 110UL);
#if (BOARD_LOW_VOLTAGE_DIRECT_TEST == 0U)
    CHECK(sm.state == SYSTEM_STATE_STANDBY, "bypass delay not elapsed blocks RUN");
    RunSupervisor_Service(&rs, &sm, 1U, RUN_AT_TICK);
    CHECK(sm.state == SYSTEM_STATE_STANDBY, "alpha incomplete blocks RUN after delay");
#else
    CHECK(sm.state == SYSTEM_STATE_STANDBY, "alpha incomplete blocks RUN");
#endif
    g_switch_alpha = 1.0f; g_trip_clear = 0;
    RunSupervisor_Service(&rs, &sm, 1U, RUN_AT_TICK);
    CHECK(sm.state == SYSTEM_STATE_STANDBY, "TZ fault blocks RUN");
    g_trip_clear = 1;
    RunSupervisor_Service(&rs, &sm, 1U, RUN_AT_TICK + 20UL);
    CHECK(sm.state == SYSTEM_STATE_RUN && g_release_calls == 1 && g_led,
          "releases PWM only after PLL + TZ gates");
    CHECK(g_grid && g_bypass, "RUN keeps grid+bypass closed together");
    {
        int i, grid_on = -1, bypass_on = -1, release = -1;
        for (i = 0; i < g_trace_count; i++) {
            if (g_trace[i] == ACT_GRID_ON && grid_on < 0) grid_on = i;
            if (g_trace[i] == ACT_BYPASS_ON && bypass_on < 0) bypass_on = i;
            if (g_trace[i] == ACT_RELEASE && release < 0) release = i;
        }
        CHECK(grid_on >= 0 && bypass_on >= 0 && release > grid_on && release > bypass_on,
              "START order is grid -> bypass -> PLL wait -> PWM release");
    }
    CHECK(g_released_phase == CTRL_TEST_PHASE_A && g_active_phase == CTRL_TEST_PHASE_A,
          "default run latches and releases A phase");
    g_ctrl_test_phase = CTRL_TEST_PHASE_B;
    CHECK(g_active_phase == CTRL_TEST_PHASE_A,
          "changing requested phase in RUN does not hot-switch active phase");
    g_ctrl_run_mode = CTRL_RUN_MODE_THREE_PHASE;
    CHECK(g_active_mode == CTRL_RUN_MODE_SINGLE_PHASE,
          "changing requested mode in RUN does not hot-switch active mode");
}

static void test_three_phase_and_invalid_mode(void)
{
    RunSupervisor rs; StateMachine sm;
    reset_fixture(&rs, &sm); g_ctrl_run_mode = CTRL_RUN_MODE_THREE_PHASE;
    begin_run_wait(&rs, &sm, 100UL);
    g_pll_switch_req = 1U; g_switch_alpha = 1.0f;
    RunSupervisor_Service(&rs, &sm, 1U, RUN_AT_TICK);
    CHECK(sm.state == SYSTEM_STATE_RUN && g_released_phase == 0 &&
          g_active_mode == CTRL_RUN_MODE_THREE_PHASE,
          "three-phase mode releases all six PWM modules");

    reset_fixture(&rs, &sm); g_ctrl_run_mode = 9U;
    RunSupervisor_Service(&rs, &sm, 1U, 100UL);
    CHECK(sm.state == SYSTEM_STATE_STANDBY && g_release_calls == 0 &&
          g_pwm_blocked && !g_grid && !g_bypass && !g_led,
          "invalid run mode denies RUN and keeps complete safe state");
}

static void test_phase_selection_and_invalid_safe(void)
{
    RunSupervisor rs; StateMachine sm;
    reset_fixture(&rs, &sm);
    g_ctrl_test_phase = CTRL_TEST_PHASE_C;
    begin_run_wait(&rs, &sm, 100UL);
    g_pll_switch_req = 1U; g_switch_alpha = 1.0f;
    RunSupervisor_Service(&rs, &sm, 1U, RUN_AT_TICK);
    CHECK(sm.state == SYSTEM_STATE_RUN && g_released_phase == CTRL_TEST_PHASE_C,
          "valid C request is latched and only C is released");

    reset_fixture(&rs, &sm);
    g_ctrl_test_phase = 9U;
    RunSupervisor_Service(&rs, &sm, 1U, 100UL);
    g_pll_switch_req = 1U; g_switch_alpha = 1.0f;
    RunSupervisor_Service(&rs, &sm, 1U, 120UL);
    CHECK(sm.state == SYSTEM_STATE_STANDBY && g_release_calls == 0 && g_pwm_blocked &&
          !g_grid && !g_bypass,
          "invalid phase keeps PWM and GPIO22/GPIO23 off and denies RUN");
}

static void test_stop_each_phase(void)
{
    RunSupervisor rs; StateMachine sm;
    reset_fixture(&rs, &sm); begin_run_wait(&rs, &sm, 100UL);
    check_stop_order(&rs, &sm, 110UL, "DIRECT WAIT STOP trace");

    reset_fixture(&rs, &sm); begin_run_wait(&rs, &sm, 100UL);
    g_pll_switch_req = 1U; g_switch_alpha = 1.0f;
    RunSupervisor_Service(&rs, &sm, 1U, RUN_AT_TICK);
    CHECK(sm.state == SYSTEM_STATE_RUN, "fixture reaches RUN");
    check_stop_order(&rs, &sm, RUN_AT_TICK + 20UL, "RUN STOP trace");
}

static void test_fault_safe(void)
{
    RunSupervisor rs; StateMachine sm;
    reset_fixture(&rs, &sm); begin_run_wait(&rs, &sm, 100UL);
    System_EnterFault(&sm, FAULT_HW_TZ_TRIP, 110UL);
    RunSupervisor_Service(&rs, &sm, 1U, 120UL);
    CHECK(sm.state == SYSTEM_STATE_FAULT && rs.restart_inhibit,
          "FAULT remains latched and inhibits restart");
    CHECK(g_pwm_blocked && !g_grid && !g_bypass && !g_led,
          "FAULT opens precharge power path");
}

int main(void)
{
#if (BOARD_PLL_RELAY_TEST_ONLY != 0U)
    RunSupervisor rs; StateMachine sm;
    printf("=== PLL/Relay Bench-Safe Supervisor Tests ===\n");
    reset_fixture(&rs, &sm);
    RunSupervisor_Service(&rs, &sm, 1U, 100UL);
    CHECK(sm.state == SYSTEM_STATE_STANDBY, "bench mode never enters RUN");
    CHECK(g_pwm_blocked && g_release_calls == 0 && g_led,
          "bench mode keeps PWM OST while GPIO20 follows pressed GPIO21");
    CHECK(g_grid && g_bypass, "GPIO21 closes GPIO22/GPIO23 for relay test only");
    g_pll_switch_req = 1U; g_switch_alpha = 1.0f;
    RunSupervisor_Service(&rs, &sm, 1U, 120UL);
    CHECK(sm.state == SYSTEM_STATE_STANDBY && g_release_calls == 0 && g_pwm_blocked,
          "PLL lock cannot release PWM in bench mode");
    RunSupervisor_Service(&rs, &sm, 0U, 130UL);
    CHECK(!g_grid && !g_bypass && g_pwm_blocked && !g_led,
          "GPIO21 low opens both relays and turns GPIO20 off while PWM stays blocked");
#else
    printf("=== Run Supervisor Start Sequence Tests ===\n");
    test_powerup_and_direct_wait();
    test_run_gates();
    test_stop_each_phase();
    test_fault_safe();
    test_phase_selection_and_invalid_safe();
    test_three_phase_and_invalid_mode();
#endif
    printf("=== %s ===\n", g_failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED");
    return g_failures ? 1 : 0;
}

#endif
