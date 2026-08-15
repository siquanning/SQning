#include <math.h>
#include <stdio.h>
#include "firmware/bsp/board_config.h"
#include "firmware/control/control_global.h"
#include "firmware/control/control_closedloop.h"
#include "firmware/services/measurement.h"
#include "firmware/services/justfloat.h"

PLL_State g_pll;
volatile uint16_t g_pll_switch_req;
volatile float g_switch_alpha;
volatile float g_switch_phase_err_deg;

volatile PhaseClosedLoopState g_phase_ctrl[3];
static uint16_t g_active_mode = CTRL_RUN_MODE_THREE_PHASE;
static uint16_t g_active_phase = CTRL_TEST_PHASE_A;
MeasurementSample g_measurement;

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", (m)); failures++; } } while (0)

void DrvSci_SendByte(uint16_t byte) { (void)byte; }
uint16_t ClosedLoop_GetActiveRunMode(void) { return g_active_mode; }
uint16_t ClosedLoop_GetActivePhase(void) { return g_active_phase; }
uint16_t ClosedLoop_IsValidTestPhase(uint16_t p) { return p >= 1U && p <= 3U; }

static void test_closedloop_channels_follow_current_phase_snapshot(void)
{
    float ch[JUSTFLOAT_CH_COUNT];
    uint16_t phase;

    for (phase = CTRL_TEST_PHASE_A; phase <= CTRL_TEST_PHASE_C; phase++) {
        float base = 100.0f * (float)phase;
        uint16_t i = phase - CTRL_TEST_PHASE_A;
        g_phase_ctrl[i].vac = base; g_phase_ctrl[i].iac = base+1.0f;
        g_phase_ctrl[i].iref = base+2.0f; g_phase_ctrl[i].vdc_avg = base+3.0f;
        g_phase_ctrl[i].vdc_ref_ramp = base+4.0f; g_phase_ctrl[i].iamp = base+5.0f;
        g_phase_ctrl[i].m = base+6.0f; g_phase_ctrl[i].theta_phase = base+7.0f;
        g_debug_phase = phase;

        JustFloat_GetChannels(DEBUG_VIEW_CLOSEDLOOP, ch);
        CHECK(ch[0] == base && ch[1] == base + 1.0f &&
              ch[2] == base + 2.0f && ch[3] == base + 3.0f &&
              ch[4] == base + 4.0f && ch[5] == base + 5.0f &&
              ch[6] == base + 6.0f && ch[7] == base + 7.0f,
              "closed-loop CH0-7 follow active-phase public snapshot");
    }
    g_debug_phase = CTRL_TEST_PHASE_C;
    CHECK(g_active_mode == CTRL_RUN_MODE_THREE_PHASE &&
          g_active_phase == CTRL_TEST_PHASE_A,
          "debug phase changes observation only, never control selection");
}

static void test_pll_channels_and_invalid_view_fallback(void)
{
    float ch[JUSTFLOAT_CH_COUNT];

    g_measurement.vac_v[0] = 1.0f;
    g_measurement.vac_v[1] = 2.0f;
    g_measurement.vac_v[2] = 3.0f;
    g_pll.theta = 4.0f; g_pll.freq = 5.0f; g_pll.vq = 6.0f;
    g_pll.vmag = 7.0f; g_switch_alpha = 8.0f;
    JustFloat_GetChannels(DEBUG_VIEW_PLL, ch);
    CHECK(ch[0] == 1.0f && ch[1] == 2.0f && ch[2] == 3.0f && ch[3] == 4.0f &&
          ch[4] == 5.0f && ch[5] == 6.0f && ch[6] == 7.0f && ch[7] == 8.0f,
          "PLL CH0-7 mapping is stable");

    g_debug_phase = CTRL_TEST_PHASE_A;
    g_phase_ctrl[0].vac=11.0f; g_phase_ctrl[0].theta_phase=18.0f;
    JustFloat_GetChannels(99U, ch);
    CHECK(ch[0] == 11.0f && ch[7] == 18.0f,
          "invalid view falls back to configured default closed-loop view");
}

int main(void)
{
    printf("=== JustFloat Active Phase Tests ===\n");
    test_closedloop_channels_follow_current_phase_snapshot();
    test_pll_channels_and_invalid_view_fallback();
    printf("=== %s ===\n", failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED");
    return failures ? 1 : 0;
}
