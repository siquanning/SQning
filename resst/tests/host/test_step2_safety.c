#ifdef __TMS320C28XX__
static int _host_test_placeholder_step2;
#else

#include <stdio.h>
#include <stdint.h>

static int g_failures = 0;

#define FAIL(msg) do { \
    printf("FAIL: %s\n", (msg)); fflush(stdout); \
    g_failures++; \
} while(0)

/* ---- Replica of driver ClampU16 for host validation ---- */
static uint16_t RefClampU16(uint32_t val, uint16_t lo, uint16_t hi)
{
    if (val < (uint32_t)lo) return lo;
    if (val > (uint32_t)hi) return hi;
    return (uint16_t)val;
}

/*
 * Replica of comparison upper bound calculation:
 *   hi = (uint16_t)(((uint32_t)tbprd * (uint32_t)max_duty_permill) / 1000U)
 */
static uint16_t RefCmpHi(uint16_t tbprd, uint16_t max_duty_permill)
{
    return (uint16_t)(((uint32_t)tbprd * (uint32_t)max_duty_permill) / 1000U);
}

/* ==================================================================
 * T1: ClampU16 boundary tests
 * ================================================================== */
static void test_clamp_basic(void)
{
    /* Value within range */
    if (RefClampU16(100U, 0U, 200U) != 100U)
        FAIL("T1.1: mid-range value should pass through");

    /* Value below lo */
    if (RefClampU16(0U, 50U, 200U) != 50U)
        FAIL("T1.2: below lo should clamp to lo");

    /* Value above hi */
    if (RefClampU16(300U, 50U, 200U) != 200U)
        FAIL("T1.3: above hi should clamp to hi");

    /* Value at lo boundary */
    if (RefClampU16(50U, 50U, 200U) != 50U)
        FAIL("T1.4: at lo boundary should pass through");

    /* Value at hi boundary */
    if (RefClampU16(200U, 50U, 200U) != 200U)
        FAIL("T1.5: at hi boundary should pass through");
}

/* ==================================================================
 * T2: ClampU16 zero-range
 * ================================================================== */
static void test_clamp_zero_range(void)
{
    /* lo == hi == 0 */
    if (RefClampU16(0U, 0U, 0U) != 0U)
        FAIL("T2.1: zero range, val=0 should return 0");

    if (RefClampU16(100U, 0U, 0U) != 0U)
        FAIL("T2.2: zero range hi=0, val above should clamp to 0");
}

/* ==================================================================
 * T3: ClampU16 16-bit full range
 * ================================================================== */
static void test_clamp_full_range(void)
{
    /* Full Uint16 range */
    if (RefClampU16(0U, 0U, 65535U) != 0U)
        FAIL("T3.1: full range, val=0");

    if (RefClampU16(65535U, 0U, 65535U) != 65535U)
        FAIL("T3.2: full range, val=65535");

    /* 32-bit overflow check: val > 65535 with hi=65535 */
    if (RefClampU16(100000UL, 0U, 65535U) != 65535U)
        FAIL("T3.3: 32-bit val > u16 max should clamp to hi");
}

/* ==================================================================
 * T4: Safe compare upper bound calculation
 * ================================================================== */
static void test_cmp_hi_calc(void)
{
    uint16_t tbprd, max_permill, expected, result;

    /* BOARD_PWM_TBPRD = 150M / (2*60k) = 1250, max_duty = 480 per-mill */
    tbprd = 1250U;
    max_permill = 480U;
    expected = 600U;   /* 1250 * 480 / 1000 = 600 */
    result = RefCmpHi(tbprd, max_permill);
    if (result != expected)
    {
        printf("  T4.1: got %u, expected %u\n", (unsigned)result, (unsigned)expected);
        FAIL("T4.1: TBPRD=1250, 48.0% → hi=600");
    }

    /* Zero duty: hi should be 0 */
    result = RefCmpHi(tbprd, 0U);
    if (result != 0U)
        FAIL("T4.2: 0% duty → hi=0");

    /* Full duty 1000 per-mill */
    result = RefCmpHi(tbprd, 1000U);
    if (result != tbprd)
        FAIL("T4.3: 100.0% duty → hi=TBPRD");

    /* Small TBPRD edge case */
    tbprd = 1U;
    result = RefCmpHi(tbprd, 500U);
    expected = 0U;  /* 1 * 500 / 1000 = 0 (integer truncation) */
    if (result != expected)
        FAIL("T4.4: TBPRD=1, 50% → hi=0 (truncation)");
}

/* ==================================================================
 * T5: ADC raw → clamped safe compare mapping
 * ================================================================== */
static void test_adc_to_safe_cmp(void)
{
    uint16_t tbprd, max_permill, hi, raw, clamped;

    /* Test with typical Step 2 values */
    tbprd = 1250U;        /* 60kHz up-down at 150MHz */
    max_permill = 480U;   /* 48.0% hard clamp */
    hi = RefCmpHi(tbprd, max_permill);  /* hi = 600 */

    /* ADC raw = 0 → clamp to 0 */
    raw = 0U;
    clamped = RefClampU16((uint32_t)raw, 0U, hi);
    if (clamped != 0U)
        FAIL("T5.1: raw=0 → cmp=0");

    /* ADC raw = mid-scale (2048) → clamp to hi */
    raw = 2048U;
    clamped = RefClampU16((uint32_t)raw, 0U, hi);
    if (clamped != hi)
        FAIL("T5.2: raw=2048 > hi → clamp to hi");

    /* ADC raw = half of hi (300) → pass through */
    raw = 300U;
    clamped = RefClampU16((uint32_t)raw, 0U, hi);
    if (clamped != 300U)
        FAIL("T5.3: raw=300 <= hi → pass through");

    /* ADC raw = 4095 (max) → clamp to hi */
    raw = 4095U;
    clamped = RefClampU16((uint32_t)raw, 0U, hi);
    if (clamped != hi)
        FAIL("T5.4: raw=4095 > hi → clamp to hi");

    /* ADC raw at hi boundary */
    raw = hi;   /* 600 */
    clamped = RefClampU16((uint32_t)raw, 0U, hi);
    if (clamped != hi)
        FAIL("T5.5: raw==hi → pass through at boundary");
}

/* ==================================================================
 * T6: Invalid/missing input guard
 * ================================================================== */
static void test_invalid_input(void)
{
    uint16_t tbprd, max_permill, hi;

    /*
     * DrvAdc_ReadRaw returns -1 for channel >= 16.
     * App_AdcIsr guards with: sample = (raw >= 0) ? (uint16_t)raw : 0U.
     * Verify this guard pattern.
     */
    int32_t bad_raw = -1;
    uint16_t sample = (bad_raw >= 0) ? (uint16_t)bad_raw : 0U;
    if (sample != 0U)
        FAIL("T6.1: ADC error (-1) should default sample to 0");

    bad_raw = -100;
    sample = (bad_raw >= 0) ? (uint16_t)bad_raw : 0U;
    if (sample != 0U)
        FAIL("T6.2: ADC error (-100) should default sample to 0");

    /* Sample = 0 → CMP = 0 (safe, no PWM output even if enabled) */
    tbprd = 1250U;
    max_permill = 480U;
    hi = RefCmpHi(tbprd, max_permill);
    sample = 0U;
    if (RefClampU16((uint32_t)sample, 0U, hi) != 0U)
        FAIL("T6.3: zero sample → CMP=0");

    /* Sample beyond max ADC range → still clamped by duty limit, not by ADC range */
    sample = 65535U;
    if (RefClampU16((uint32_t)sample, 0U, hi) != hi)
        FAIL("T6.4: overflow sample → clamp to hi");
}

/* ==================================================================
 * T7: max_duty_permill at boundary values
 * ================================================================== */
static void test_duty_permill_boundaries(void)
{
    uint16_t tbprd, max_permill, hi;

    tbprd = 1250U;

    /* 100% duty (1000 per-mill) */
    max_permill = 1000U;
    hi = RefCmpHi(tbprd, max_permill);
    if (hi != tbprd)
        FAIL("T7.1: 1000 per-mill → hi=TBPRD");

    /* 0.1% duty (1 per-mill) */
    max_permill = 1U;
    hi = RefCmpHi(tbprd, max_permill);
    if (hi != 1U)
        FAIL("T7.2: 1 per-mill of 1250 → hi=1");

    /* 0% duty (0 per-mill) */
    max_permill = 0U;
    hi = RefCmpHi(tbprd, max_permill);
    if (hi != 0U)
        FAIL("T7.3: 0 per-mill → hi=0");
}

int main(void)
{
    printf("=== Step 2 Safety Shadow Mapping Tests ===\n\n");

    test_clamp_basic();
    test_clamp_zero_range();
    test_clamp_full_range();
    test_cmp_hi_calc();
    test_adc_to_safe_cmp();
    test_invalid_input();
    test_duty_permill_boundaries();

    printf("\n=== %s ===\n", (g_failures == 0) ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return (g_failures > 0) ? 1 : 0;
}

#endif /* !__TMS320C28XX__ */
