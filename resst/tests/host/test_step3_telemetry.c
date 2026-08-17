#ifdef __TMS320C28XX__
static int _host_test_placeholder_step3_telemetry;
#else

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "firmware/app/telemetry.h"

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
 * T1: Init zeroes everything
 * ================================================================== */
static void test_init(void)
{
    Telemetry t;
    uint8_t raw[sizeof(Telemetry)];
    memset(raw, 0xFF, sizeof(raw));
    memcpy(&t, raw, sizeof(Telemetry));

    Telemetry_Init(&t);

    ASSERT_EQ(t.active_idx, 0U, "T1.1: active_idx=0");
    ASSERT_EQ(t.read_idx, 0U, "T1.2: read_idx=0");
    ASSERT_EQ(t.overrun_count, 0UL, "T1.3: overrun_count=0");
    ASSERT_EQ(t.write_count, 0UL, "T1.4: write_count=0");
    ASSERT_EQ(t.buffer[0].version, 0U, "T1.5: buffer[0].version=0");
    ASSERT_EQ(t.buffer[1].version, 0U, "T1.6: buffer[1].version=0");
}

/* ==================================================================
 * T2: Write → Read consistent snapshot
 * ================================================================== */
static void test_write_read_consistent(void)
{
    Telemetry t;
    TelemetryFastSnapshot out;
    uint16_t adc[2] = { 2048U, 1000U };
    uint16_t cmpa[3] = { 300U, 0U, 0U };
    uint16_t cmpb[3] = { 250U, 0U, 0U };

    Telemetry_Init(&t);

    /* Write a snapshot */
    Telemetry_WriteFastSnapshot(&t, 3U, adc,
                                cmpa, cmpb,
                                1U,            /* output_valid */
                                0U,            /* trip_flags */
                                0U,            /* fault_code */
                                100U);         /* step_count */

    /* Read it back */
    ASSERT_TRUE(Telemetry_ReadSnapshot(&t, &out) == 1, "T2.1: consistent read");

    ASSERT_EQ(out.state, 3U, "T2.2: state=3 (RUN)");
    ASSERT_EQ(out.adc_raw[0], 2048U, "T2.3: adc_raw[0]");
    ASSERT_EQ(out.adc_raw[1], 1000U, "T2.4: adc_raw[1]");
    ASSERT_EQ(out.cmpa[0], 300U, "T2.5: cmpa");
    ASSERT_EQ(out.cmpb[0], 250U, "T2.6: cmpb");
    ASSERT_EQ(out.output_valid, 1U, "T2.7: output_valid");
    ASSERT_EQ(out.trip_flags, 0U, "T2.8: trip_flags=0");
    ASSERT_EQ(out.fault_code, 0U, "T2.9: fault_code=0");
    ASSERT_EQ(out.step_count, 100U, "T2.10: step_count");

    ASSERT_EQ(t.write_count, 1UL, "T2.11: write_count=1");
}

/* ==================================================================
 * T3: Version increments on each write
 * ================================================================== */
static void test_version_increment(void)
{
    Telemetry t;
    TelemetryFastSnapshot out;
    uint16_t adc[2] = { 100U, 200U };
    uint16_t cmpa[3] = { 100U, 0U, 0U };
    uint16_t cmpb[3] = { 200U, 0U, 0U };
    uint16_t i;

    Telemetry_Init(&t);

    for (i = 0U; i < 5U; i++)
    {
        Telemetry_WriteFastSnapshot(&t, 3U, adc,
                                    cmpa, cmpb, 1U, 0U, 0U, (uint16_t)i);
    }

    ASSERT_TRUE(Telemetry_ReadSnapshot(&t, &out) == 1, "T3.1: read after 5 writes");
    ASSERT_TRUE(out.version > 0U, "T3.2: version > 0");
    ASSERT_EQ(t.write_count, 5UL, "T3.3: write_count=5");
}

/* ==================================================================
 * T4: Overrun detection when reading same buffer ISR writes to
 * ================================================================== */
static void test_overrun_detection(void)
{
    Telemetry t;
    TelemetryFastSnapshot out;
    uint16_t adc[2] = { 500U, 600U };
    uint16_t cmpa[3] = { 100U, 0U, 0U };
    uint16_t cmpb[3] = { 200U, 0U, 0U };

    Telemetry_Init(&t);

    /* Write 3x to buffer 0 (active_idx stays 0 while read_idx stays 0) */
    Telemetry_WriteFastSnapshot(&t, 3U, adc, cmpa, cmpb, 1U, 0U, 0U, 1U);
    ASSERT_EQ(t.overrun_count, 1UL, "T4.1: overrun on first write (read_idx==active_idx)");

    Telemetry_WriteFastSnapshot(&t, 3U, adc, cmpa, cmpb, 1U, 0U, 0U, 2U);
    ASSERT_EQ(t.overrun_count, 2UL, "T4.2: overrun_count=2");

    /* Read should get the latest buffer contents */
    ASSERT_TRUE(Telemetry_ReadSnapshot(&t, &out) == 1, "T4.3: consistent read after writes");
}

/* ==================================================================
 * T5: Multiple write slots — version resets per buffer
 * ================================================================== */
static void test_multiple_snapshots(void)
{
    Telemetry t;
    TelemetryFastSnapshot out1, out2;
    uint16_t adc1[2] = { 100U, 200U };
    uint16_t adc2[2] = { 300U, 400U };
    uint16_t cmpa1[3] = { 500U, 0U, 0U }, cmpb1[3] = { 600U, 0U, 0U };
    uint16_t cmpa2[3] = { 700U, 0U, 0U }, cmpb2[3] = { 800U, 0U, 0U };
    uint16_t i;

    Telemetry_Init(&t);

    /* Write 10 snapshots to fill buffer with known data */
    for (i = 0U; i < 10U; i++)
    {
        Telemetry_WriteFastSnapshot(&t, 3U, adc1, cmpa1, cmpb1, 1U, 0U, 0U, i);
    }

    /* Read a snapshot — should contain latest data */
    ASSERT_TRUE(Telemetry_ReadSnapshot(&t, &out1) == 1, "T5.1: read first snapshot");

    /* Write more with different data */
    for (i = 0U; i < 5U; i++)
    {
        Telemetry_WriteFastSnapshot(&t, 4U, adc2, cmpa2, cmpb2, 0U, 1U, 10U, i);
    }

    /* Read second snapshot — should contain new data */
    ASSERT_TRUE(Telemetry_ReadSnapshot(&t, &out2) == 1, "T5.2: read second snapshot");
    ASSERT_EQ(out2.state, 4U, "T5.3: state=4 (FAULT)");
    ASSERT_EQ(out2.trip_flags, 1U, "T5.4: trip_flags=1");
    ASSERT_EQ(out2.fault_code, 10U, "T5.5: fault_code=10");
}

/* ==================================================================
 * T6: NULL pointer guards
 * ================================================================== */
static void test_null_guards(void)
{
    uint16_t adc[2] = { 0U, 0U };
    uint16_t cmp[3] = { 0U, 0U, 0U };
    TelemetryFastSnapshot out;

    Telemetry_Init(((Telemetry *)0));
    Telemetry_WriteFastSnapshot(((Telemetry *)0), 0U, adc, cmp, cmp, 0U, 0U, 0U, 0U);
    Telemetry_ReadSnapshot(((Telemetry *)0), &out);
    Telemetry_ReadSnapshot(&((Telemetry){0}), ((TelemetryFastSnapshot *)0));
    /* All null-guarded */
}

int main(void)
{
    printf("=== Step 3 Telemetry Host Tests ===\n\n");

    test_init();
    test_write_read_consistent();
    test_version_increment();
    test_overrun_detection();
    test_multiple_snapshots();
    test_null_guards();

    printf("\n=== %s ===\n", (g_failures == 0) ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return (g_failures > 0) ? 1 : 0;
}

#endif /* !__TMS320C28XX__ */
