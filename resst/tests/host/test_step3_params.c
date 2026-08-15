#ifdef __TMS320C28XX__
static int _host_test_placeholder_step3_params;
#else

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "firmware/platform_profile.h"
#include "firmware/app/param_manager.h"

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

/* ==================================================================
 * T1: Param_Init sets safe defaults
 * ================================================================== */
static void test_param_init(void)
{
    ParamManager pm;
    Param_Init(&pm, 1250U);

    ASSERT_EQ(pm.active.version, 1U, "T1.1: active.version=1");
    ASSERT_EQ(pm.active.m_permill[0], 0U, "T1.2: m[0]=0");
    ASSERT_EQ(pm.active.tbprd, 1250U, "T1.3: tbprd=1250");
    ASSERT_EQ(pm.active.adc_safe_min, 1U, "T1.4: adc_safe_min=1");
    ASSERT_EQ(pm.active.adc_safe_max, 4094U, "T1.5: adc_safe_max=4094");
    ASSERT_EQ(pm.commit_count, 0UL, "T1.6: commit_count=0");
    ASSERT_EQ(pm.reject_count, 0UL, "T1.7: reject_count=0");

    /* Pending should have version 2 */
    ASSERT_EQ(pm.pending.version, 2U, "T1.8: pending.version=2");
}

/* ==================================================================
 * T2: Param_Validate rejects version ≤ active
 * ================================================================== */
static void test_validate_version(void)
{
    ControlParams active;
    ControlParams pending;

    memset(&active, 0, sizeof(active));
    active.version = 5U;
    active.m_permill[0] = 0;
    active.control_mode = 0U;
    active.tbprd = 1250U;
    active.adc_safe_min = 1U;
    active.adc_safe_max = 4094U;
    active.fault_thresh_adc_stuck = 10U;

    /* Same version → reject */
    pending = active;
    ASSERT_EQ(Param_Validate(&pending, &active), PARAM_REJECT_VERSION,
              "T2.1: same version → REJECT_VERSION");

    /* Lower version → reject */
    pending.version = 3U;
    ASSERT_EQ(Param_Validate(&pending, &active), PARAM_REJECT_VERSION,
              "T2.2: lower version → REJECT_VERSION");

    /* Higher version → pass */
    pending.version = 6U;
    ASSERT_EQ(Param_Validate(&pending, &active), PARAM_REJECT_NONE,
              "T2.3: higher version → ok");
}

/* ==================================================================
 * T3: Param_Validate rejects out-of-range values
 * ================================================================== */
static void test_validate_ranges(void)
{
    ControlParams active;
    ControlParams p;

    memset(&active, 0, sizeof(active));
    active.version = 1U;
    active.m_permill[0] = 0;
    active.control_mode = 0U;
    active.tbprd = 1250U;
    active.adc_safe_min = 1U;
    active.adc_safe_max = 4094U;
    active.fault_thresh_adc_stuck = 10U;

    p = active;
    p.version = 2U;

    /* Modulation below minimum → reject */
    p.m_permill[0] = -981;
    ASSERT_EQ(Param_Validate(&p, &active), PARAM_REJECT_M_RANGE,
              "T3.1: m=-981 → REJECT");

    /* Modulation above maximum → reject */
    p.m_permill[0] = 981;
    ASSERT_EQ(Param_Validate(&p, &active), PARAM_REJECT_M_RANGE,
              "T3.2: m=981 → REJECT");

    p = active; p.version = 3U;

    /* Control mode = 1 → reject (only 0 supported) */
    p.control_mode = 1U;
    ASSERT_EQ(Param_Validate(&p, &active), PARAM_REJECT_CONTROL_MODE,
              "T3.3: control_mode=1 → REJECT");

    p = active; p.version = 4U;

    /* TBPRD = 0 → reject */
    p.tbprd = 0U;
    ASSERT_EQ(Param_Validate(&p, &active), PARAM_REJECT_TBPRD_RANGE,
              "T3.4: tbprd=0 → REJECT");

    p = active; p.version = 5U;

    /* adc_safe_min >= adc_safe_max → reject */
    p.adc_safe_min = 4000U;
    p.adc_safe_max = 2000U;
    ASSERT_EQ(Param_Validate(&p, &active), PARAM_REJECT_ADC_RANGE_ORDER,
              "T3.5: min>=max → REJECT");

    p = active; p.version = 6U;

    /* adc_safe_max > 4095 → reject */
    p.adc_safe_max = 5000U;
    ASSERT_EQ(Param_Validate(&p, &active), PARAM_REJECT_ADC_MAX_EXCEED,
              "T3.6: safe_max>4095 → REJECT");

    p = active; p.version = 7U;

    /* fault_thresh = 0 → reject */
    p.fault_thresh_adc_stuck = 0U;
    ASSERT_EQ(Param_Validate(&p, &active), PARAM_REJECT_THRESH_ZERO,
              "T3.7: thresh=0 → REJECT");
}

/* ==================================================================
 * T4: Commit path: submit → validate → commit
 * ================================================================== */
static void test_commit_flow(void)
{
    ParamManager pm;
    ControlParams new_params;

    Param_Init(&pm, 1250U);

    /* Prepare new params with higher version */
    memcpy(&new_params, &pm.active, sizeof(ControlParams));
    new_params.version = 3U;
    new_params.m_permill[0] = 300;
    new_params.adc_safe_min = 50U;
    new_params.adc_safe_max = 4000U;

    /* Submit to pending */
    Param_SubmitPending(&pm, &new_params);
    ASSERT_EQ(pm.pending.m_permill[0], 300U, "T4.1: pending.m[0]=300");

    /* Active should be unchanged */
    ASSERT_EQ(pm.active.m_permill[0], 0U, "T4.2: active.m[0] still 0");

    /* Request commit */
    Param_RequestCommit(&pm);
    ASSERT_EQ(pm.commit_requested, 1UL, "T4.3: commit_requested=1");

    /* Execute commit */
    ASSERT_TRUE(Param_CheckPendingCommit(&pm) == 1, "T4.4: commit success");

    /* Active updated */
    ASSERT_EQ(pm.active.m_permill[0], 300U, "T4.5: active.m[0]=300");
    ASSERT_EQ(pm.active.version, 3U, "T4.6: active.version=3");
    ASSERT_EQ(pm.commit_count, 1UL, "T4.7: commit_count=1");
    ASSERT_EQ(pm.reject_count, 0UL, "T4.8: reject_count=0");
}

/* ==================================================================
 * T5: Commit rejection counting
 * ================================================================== */
static void test_commit_reject(void)
{
    ParamManager pm;
    ControlParams bad;

    Param_Init(&pm, 1250U);

    /* Submit invalid params */
    memcpy(&bad, &pm.active, sizeof(ControlParams));
    bad.version = 3U;
    bad.m_permill[0] = 981;  /* Invalid */

    Param_SubmitPending(&pm, &bad);
    Param_RequestCommit(&pm);

    ASSERT_TRUE(Param_CheckPendingCommit(&pm) == 0, "T5.1: commit rejected");
    ASSERT_EQ(pm.reject_count, 1UL, "T5.2: reject_count=1");
    ASSERT_EQ(pm.last_reject_reason, PARAM_REJECT_M_RANGE, "T5.3: reject reason = M_RANGE");
    ASSERT_EQ(pm.commit_count, 0UL, "T5.4: commit_count still 0");
    ASSERT_EQ(pm.active.m_permill[0], 0U, "T5.5: active unchanged");
}

/* ==================================================================
 * T6: Param_ReadActive consistency
 * ================================================================== */
static void test_read_active(void)
{
    ParamManager pm;
    ControlParams out;

    Param_Init(&pm, 1250U);

    /* Modify active params */
    ControlParams new_params;
    memcpy(&new_params, &pm.active, sizeof(ControlParams));
    new_params.version = 3U;
    new_params.m_permill[0] = 350;
    Param_SubmitPending(&pm, &new_params);
    Param_RequestCommit(&pm);
    Param_CheckPendingCommit(&pm);

    /* Read active — should get new values */
    memset(&out, 0xFF, sizeof(out));
    Param_ReadActive(&pm, &out);
    ASSERT_EQ(out.m_permill[0], 350U, "T6.1: ReadActive → new m[0]");
    ASSERT_EQ(out.version, 3U, "T6.2: ReadActive → new version");
}

/* ==================================================================
 * T7: NULL guard for Param_Validate
 * ================================================================== */
static void test_validate_null_guard(void)
{
    ControlParams active;
    ControlParams p;

    memset(&active, 0, sizeof(active));
    active.version = 1U;
    memset(&p, 0, sizeof(p));

    ASSERT_EQ(Param_Validate(((const ControlParams *)0), &active),
              PARAM_REJECT_VERSION, "T7.1: NULL params → REJECT");
    ASSERT_EQ(Param_Validate(&p, ((const ControlParams *)0)),
              PARAM_REJECT_VERSION, "T7.2: NULL active → REJECT");
}

int main(void)
{
    printf("=== Step 3 Parameter Manager Host Tests ===\n\n");

    test_param_init();
    test_validate_version();
    test_validate_ranges();
    test_commit_flow();
    test_commit_reject();
    test_read_active();
    test_validate_null_guard();

    printf("\n=== %s ===\n", (g_failures == 0) ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return (g_failures > 0) ? 1 : 0;
}

#endif /* !__TMS320C28XX__ */
