#ifdef __TMS320C28XX__
static int _host_test_placeholder_pll;
#else

#include <math.h>
#include <stdio.h>
#include "firmware/bsp/board_config.h"
#include "firmware/control/control_pll.h"

#define TEST_PI      3.14159265358979323846f
#define TEST_TWO_PI  (2.0f * TEST_PI)
#define TEST_TS      0.00005f

static int g_failures = 0;

#define ASSERT_TRUE(cond, label) do { \
    if (!(cond)) { printf("FAIL: %s\n", (label)); g_failures++; } \
} while (0)

#define ASSERT_NEAR(actual, expected, tolerance, label) do { \
    float _a = (actual); \
    float _e = (expected); \
    if (fabsf(_a - _e) > (tolerance)) { \
        printf("FAIL: %s (got %.6f, expected %.6f +/- %.6f)\n", \
               (label), _a, _e, (float)(tolerance)); \
        g_failures++; \
    } \
} while (0)

static void run_balanced_grid(PLL_State *pll, float freq_hz,
                              float amplitude_v, float duration_s)
{
    unsigned long i;
    unsigned long steps = (unsigned long)(duration_s / TEST_TS);
    float omega = TEST_TWO_PI * freq_hz;

    for (i = 0UL; i < steps; ++i) {
        float phase = omega * ((float)i * TEST_TS);
        float va = amplitude_v * sinf(phase);
        float vb = amplitude_v * sinf(phase - 2.0f * TEST_PI / 3.0f);
        float vc = amplitude_v * sinf(phase + 2.0f * TEST_PI / 3.0f);
        PLL_Run(pll, va, vb, vc, TEST_TS);
    }
}

static void test_init_and_holdover(void)
{
    PLL_State pll;
    unsigned i;

    PLL_Init(&pll);
    ASSERT_NEAR(pll.theta, 0.0f, 0.000001f, "init theta");
    ASSERT_NEAR(pll.freq, 50.0f, 0.0001f, "init frequency");

    for (i = 0U; i < 400U; ++i) {
        PLL_Run(&pll, 0.0f, 0.0f, 0.0f, TEST_TS);
    }

    ASSERT_NEAR(pll.freq, 50.0f, 0.001f, "low-voltage holdover frequency");
    ASSERT_NEAR(pll.vmag, 0.0f, 0.001f, "low-voltage magnitude");
    ASSERT_TRUE(pll.theta >= 0.0f && pll.theta < TEST_TWO_PI,
                "holdover theta stays wrapped");
}

static void test_grid_lock(void)
{
    PLL_State pll;
    const float amplitude = BOARD_PLL_LOCK_VMAG_MIN_V * 1.5f;

    PLL_Init(&pll);
    run_balanced_grid(&pll, 50.0f, amplitude, 0.5f);

    ASSERT_NEAR(pll.freq, 50.0f, 0.15f, "50 Hz tracking");
    ASSERT_NEAR(pll.vmag, amplitude, 0.02f, "grid magnitude above configured threshold");
    ASSERT_TRUE(fabsf(pll.vq) < 0.03f * pll.vmag,
                "q-axis error meets lock threshold");
    ASSERT_TRUE(pll.vd > 0.9f * pll.vmag,
                "d-axis polarity meets lock threshold");
}

static void test_frequency_tracking_and_dropout(void)
{
    PLL_State pll;
    const float amplitude = BOARD_PLL_LOCK_VMAG_MIN_V * 1.5f;

    PLL_Init(&pll);
    run_balanced_grid(&pll, 52.0f, amplitude, 0.7f);
    ASSERT_NEAR(pll.freq, 52.0f, 0.2f, "52 Hz tracking");

    PLL_Run(&pll, 0.0f, 0.0f, 0.0f, TEST_TS);
    ASSERT_NEAR(pll.freq, 50.0f, 0.001f, "dropout returns to holdover");
    ASSERT_NEAR(pll.pll_i, 0.0f, 0.001f, "dropout clears integrator");
}

int main(void)
{
    printf("=== SRF-PLL Host Tests ===\n");
    test_init_and_holdover();
    test_grid_lock();
    test_frequency_tracking_and_dropout();
    printf("=== %s ===\n", g_failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return g_failures == 0 ? 0 : 1;
}

#endif /* !__TMS320C28XX__ */
