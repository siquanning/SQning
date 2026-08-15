#ifdef __TMS320C28XX__
static int _host_test_placeholder_step3_control;
#else

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "firmware/control/control_common.h"
#include "firmware/control/safe_openloop.h"
#include "firmware/control/control_faststep.h"

static int g_failures = 0;

#define FAIL(msg) do { \
    printf("FAIL: %s\n", (msg)); fflush(stdout); \
    g_failures++; \
} while(0)

#define ASSERT_EQ(actual, expected, label) do { \
    unsigned _a = (unsigned)(actual); \
    unsigned _e = (unsigned)(expected); \
    if (_a != _e) { \
        printf("  %s: got %u, expected %u\n", (label), _a, _e); \
        FAIL(label); \
    } \
} while(0)

#define ASSERT_TRUE(cond, label) do { \
    if (!(cond)) { printf("  %s: expected true\n", label); FAIL(label); } \
} while(0)

#define ASSERT_FALSE(cond, label) do { \
    if (cond) { printf("  %s: expected false\n", label); FAIL(label); } \
} while(0)

/* ==================================================================
 * T1: SafeOpenLoop_MapChannel — normal operation
 * ================================================================== */
static void test_sol_normal(void)
{
    SafeOpenLoopResult r;
    uint16_t tbprd = 1250U;          /* 60kHz, 150MHz */
    uint16_t max_duty = 480U;        /* 48.0% */
    uint16_t gain = 480U;

    /* Mid-scale ADC → expected ~50% of max_duty = ~240 per-mill */
    r = SafeOpenLoop_MapChannel(2048U, tbprd, max_duty, gain);
    ASSERT_TRUE(r.valid != 0U, "T1.1: mid-scale ADC → valid");
    /* 2048 * 480 / 4095 ≈ 240 per-mill, * 1250 / 1000 ≈ 300 */
    ASSERT_EQ(r.cmp_value, 300U, "T1.2: mid-scale ADC → cmp ~300");

    /* Zero ADC → cmp 0 */
    r = SafeOpenLoop_MapChannel(0U, tbprd, max_duty, gain);
    ASSERT_TRUE(r.valid != 0U, "T1.3: zero ADC → valid");
    ASSERT_EQ(r.cmp_value, 0U, "T1.4: zero ADC → cmp 0");

    /* Max ADC (4095) → clamped to max_duty */
    r = SafeOpenLoop_MapChannel(4095U, tbprd, max_duty, gain);
    ASSERT_TRUE(r.valid != 0U, "T1.5: max ADC → valid");
    ASSERT_EQ(r.cmp_value, 600U, "T1.6: max ADC → cmp=600 (48.0% of 1250)");
}

/* ==================================================================
 * T2: SafeOpenLoop_MapChannel — boundary/edge cases
 * ================================================================== */
static void test_sol_boundary(void)
{
    SafeOpenLoopResult r;

    /* ADC = 4096 (invalid) */
    r = SafeOpenLoop_MapChannel(4096U, 1250U, 480U, 480U);
    ASSERT_EQ(r.valid, 0U, "T2.1: ADC=4096 → invalid");
    ASSERT_EQ(r.cmp_value, 0U, "T2.2: invalid → cmp=0");

    /* ADC = 65535 (overflow) */
    r = SafeOpenLoop_MapChannel(65535U, 1250U, 480U, 480U);
    ASSERT_EQ(r.valid, 0U, "T2.3: ADC=65535 → invalid");

    /* TBPRD = 0 (invalid) */
    r = SafeOpenLoop_MapChannel(2048U, 0U, 480U, 480U);
    ASSERT_EQ(r.valid, 0U, "T2.4: TBPRD=0 → invalid");

    /* max_duty = 0 */
    r = SafeOpenLoop_MapChannel(2048U, 1250U, 0U, 480U);
    ASSERT_TRUE(r.valid != 0U, "T2.5: max_duty=0 → valid");
    ASSERT_EQ(r.cmp_value, 0U, "T2.6: max_duty=0 → cmp=0");

    /* gain = 1000 (full range mapping) */
    r = SafeOpenLoop_MapChannel(2048U, 1250U, 1000U, 1000U);
    ASSERT_TRUE(r.valid != 0U, "T2.7: full gain → valid");
    /* 2048 * 1000 / 4095 ≈ 500 per-mill, * 1250 / 1000 = 625 */
    ASSERT_EQ(r.cmp_value, 625U, "T2.8: full gain → cmp=625");

    /* TBPRD minimal = 1 */
    r = SafeOpenLoop_MapChannel(2048U, 1U, 1000U, 1000U);
    ASSERT_TRUE(r.valid != 0U, "T2.9: TBPRD=1 → valid");
    /* 2048*1000/4095=500 per-mill, 1*500/1000=0 */
    ASSERT_EQ(r.cmp_value, 0U, "T2.10: TBPRD=1 truncation → cmp=0");

    /* TBPRD = 65535 */
    r = SafeOpenLoop_MapChannel(4095U, 65535U, 1000U, 1000U);
    ASSERT_TRUE(r.valid != 0U, "T2.11: TBPRD=65535 → valid");
    ASSERT_EQ(r.cmp_value, 65535U, "T2.12: max TBPRD 100% → cmp=65535");
}

/* ==================================================================
 * T3: SafeOpenLoop_MapChannel — gain clamping
 * ================================================================== */
static void test_sol_gain_clamp(void)
{
    SafeOpenLoopResult r;

    /* gain > 1000 should be clamped to 1000 */
    r = SafeOpenLoop_MapChannel(4095U, 1250U, 1000U, 2000U);
    ASSERT_TRUE(r.valid != 0U, "T3.1: oversize gain → valid");
    ASSERT_EQ(r.cmp_value, 1250U, "T3.2: gain=2000→1000, cmp=1250");

    /* max_duty_permill > 1000 should be clamped */
    r = SafeOpenLoop_MapChannel(4095U, 1250U, 2000U, 1000U);
    ASSERT_TRUE(r.valid != 0U, "T3.3: oversize max_duty → valid");
    ASSERT_EQ(r.cmp_value, 1250U, "T3.4: max_duty→1000, cmp=1250");
}

/* ==================================================================
 * T4: Control_FastStep — valid input → valid output
 * ================================================================== */
static void test_control_valid(void)
{
    ControlContext ctx;
    ControlInput input;
    ControlOutput output;

    memset(&ctx, 0, sizeof(ctx));
    ctx.m_permill[0]         = 200;
    ctx.m_permill[1]         = -200;
    ctx.m_permill[2]         = 0;
    ctx.control_mode         = 0U;
    ctx.tbprd                = 1250U;
    ctx.adc_safe_min         = 1U;
    ctx.adc_safe_max         = 4094U;
    ctx.fault_thresh_adc_stuck = 10U;

    memset(&input, 0, sizeof(input));
    input.adc_raw[0] = 2048U;
    input.adc_raw[1] = 1000U;

    memset(&output, 0xFF, sizeof(output));

    Control_FastStep(&ctx, &input, &output);

    ASSERT_TRUE(output.valid != 0U, "T4.1: valid ADC → output.valid=1");
    ASSERT_EQ(output.fault_asserted, 0U, "T4.2: valid ADC → no fault");
    ASSERT_EQ(output.fault_code, 0U, "T4.3: no fault code");
    ASSERT_EQ(ctx.step_count, 1U, "T4.4: step_count incremented");

    /* Three-phase clamped-unipolar mapping */
    ASSERT_EQ(output.force_a[0], 1U, "T4.5: positive m clamps left high");
    ASSERT_EQ(output.cmpb[0], 1000U, "T4.6: positive m chops right at 80%");
    ASSERT_EQ(output.cmpa[1], 1000U, "T4.7: negative m chops left at 80%");
    ASSERT_EQ(output.force_b[1], 1U, "T4.8: negative m clamps right high");
    ASSERT_EQ(output.force_a[2], 1U, "T4.9: zero m clamps left high");
    ASSERT_EQ(output.force_b[2], 1U, "T4.10: zero m clamps right high");
}

/* ==================================================================
 * T5: Control_FastStep — out of range input → fault
 * ================================================================== */
static void test_control_fault_input_range(void)
{
    ControlContext ctx;
    ControlInput input;
    ControlOutput output;

    memset(&ctx, 0, sizeof(ctx));
    ctx.tbprd                = 1250U;
    ctx.adc_safe_min         = 100U;
    ctx.adc_safe_max         = 4000U;
    ctx.fault_thresh_adc_stuck = 10U;

    memset(&input, 0, sizeof(input));
    input.adc_raw[0] = 10U;    /* Below safe_min=100 */
    input.adc_raw[1] = 2000U;

    Control_FastStep(&ctx, &input, &output);

    ASSERT_EQ(output.valid, 0U, "T5.1: ADC below min → invalid");
    ASSERT_EQ(output.fault_asserted, 1U, "T5.2: ADC below min → fault asserted");
    ASSERT_EQ(output.fault_code, 1U, "T5.3: fault_code=CONTROL_FAULT_INPUT_RANGE");

    /* Output compare should be 0 (safe) */
    ASSERT_EQ(output.cmpa[0], 0U, "T5.4: invalid → cmpa[0]=0");
}

/* ==================================================================
 * T6: Control_FastStep — NULL pointer guards
 * ================================================================== */
static void test_control_null_guard(void)
{
    ControlContext ctx;
    ControlInput input;
    ControlOutput output;

    /* NULL context should not crash */
    Control_FastStep(((ControlContext *)0), &input, &output);
    /* NULL input should not crash */
    Control_FastStep(&ctx, ((const ControlInput *)0), &output);
    /* NULL output should not crash */
    Control_FastStep(&ctx, &input, ((ControlOutput *)0));
    /* All null-guarded — reaching here is pass */
}

/* ==================================================================
 * T7: Control_FastStep — step_count wrap at uint16_t
 * ================================================================== */
static void test_control_step_count(void)
{
    ControlContext ctx;
    ControlInput input;
    ControlOutput output;
    uint16_t i;

    memset(&ctx, 0, sizeof(ctx));
    ctx.tbprd                = 1250U;
    ctx.adc_safe_min         = 1U;
    ctx.adc_safe_max         = 4094U;
    ctx.fault_thresh_adc_stuck = 10U;
    ctx.step_count           = 65534U;

    memset(&input, 0, sizeof(input));
    input.adc_raw[0] = 2048U;
    input.adc_raw[1] = 1000U;

    /* Call 3 times to wrap step_count */
    for (i = 0U; i < 3U; i++)
    {
        Control_FastStep(&ctx, &input, &output);
    }

    ASSERT_EQ(ctx.step_count, 1U, "T7.1: step_count wraps 65535→0→1");
}

/* ==================================================================
 * T8: Control_FastStep — stuck-ADC detection
 * ================================================================== */
static void test_control_stuck_adc(void)
{
    ControlContext ctx;
    ControlInput input;
    ControlOutput output;
    uint16_t i;

    memset(&ctx, 0, sizeof(ctx));
    ctx.tbprd                = 1250U;
    ctx.adc_safe_min         = 1U;
    ctx.adc_safe_max         = 4094U;
    ctx.fault_thresh_adc_stuck = 5U;

    memset(&input, 0, sizeof(input));
    input.adc_raw[0] = 4095U;  /* Stuck high */
    input.adc_raw[1] = 2000U;

    /* Call 4 times — threshold not yet reached */
    for (i = 0U; i < 4U; i++)
    {
        Control_FastStep(&ctx, &input, &output);
    }
    ASSERT_EQ(ctx.stuck_ctr_high, 4U, "T8.1: stuck_ctr_high=4 after 4 stuck readings");
    /* On 4th call, stuck not yet validated (thresh=5) AND input is out of ADC safe range,
     * so control marks it invalid anyway for being >4094 */
    ASSERT_EQ(output.valid, 0U, "T8.2: ADC>4094 → invalid on first call");

    /* 5th call triggers stuck fault */
    Control_FastStep(&ctx, &input, &output);
    ASSERT_EQ(ctx.stuck_ctr_high, 5U, "T8.3: stuck_ctr_high=5");
    ASSERT_EQ(output.fault_code, 2U, "T8.4: fault_code=ADC_STUCK_HIGH");
}

int main(void)
{
    printf("=== Step 3 Control / SafeOpenLoop Host Tests ===\n\n");

    test_sol_normal();
    test_sol_boundary();
    test_sol_gain_clamp();
    test_control_valid();
    test_control_fault_input_range();
    test_control_null_guard();
    test_control_step_count();
    test_control_stuck_adc();

    printf("\n=== %s ===\n", (g_failures == 0) ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return (g_failures > 0) ? 1 : 0;
}

#endif /* !__TMS320C28XX__ */
