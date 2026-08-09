#ifdef __TMS320C28XX__
static int _host_test_placeholder;
#else

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../../firmware/services/uart_frame.h"

static UartFrameContext g_ctx;
static int g_failures = 0;

#define FAIL(msg) do { \
    printf("FAIL: %s\n", (msg)); fflush(stdout); \
    g_failures++; \
} while(0)

static void diag_check(uint32_t rxBytes, uint32_t readyFrames,
                       uint32_t tooLong, uint32_t busyDrops,
                       uint32_t uartErrs, uint16_t lastErr,
                       const char *label)
{
    UartFrameDiagnostics diag;
    UartFrame_GetDiagnostics(&g_ctx, &diag);

    if (diag.rx_bytes        != rxBytes)     { printf("  %s rx_bytes: exp=%lu got=%lu\n", label, (unsigned long)rxBytes, (unsigned long)diag.rx_bytes); FAIL(label); }
    if (diag.ready_frames    != readyFrames)  { printf("  %s ready_frames: exp=%lu got=%lu\n", label, (unsigned long)readyFrames, (unsigned long)diag.ready_frames); FAIL(label); }
    if (diag.too_long_frames != tooLong)      { printf("  %s too_long: exp=%lu got=%lu\n", label, (unsigned long)tooLong, (unsigned long)diag.too_long_frames); FAIL(label); }
    if (diag.busy_drops      != busyDrops)    { printf("  %s busy_drops: exp=%lu got=%lu\n", label, (unsigned long)busyDrops, (unsigned long)diag.busy_drops); FAIL(label); }
    if (diag.uart_errors     != uartErrs)     { printf("  %s uart_errors: exp=%lu got=%lu\n", label, (unsigned long)uartErrs, (unsigned long)diag.uart_errors); FAIL(label); }
    if (diag.last_error      != lastErr)      { printf("  %s last_error: exp=%u got=%u\n", label, (unsigned)lastErr, (unsigned)diag.last_error); FAIL(label); }
}

/* ==================================================================
 * Test 1: 1-byte frame
 * ================================================================== */
static void test_1byte_frame(void)
{
    UartFrame_Init(&g_ctx);

    UartFrame_OnByte(&g_ctx, 0xAB, 100UL);

    /* at 139 ticks (39 gap) — NOT ready */
    if (UartFrame_Service(&g_ctx, 139UL) != 0) FAIL("T1: frame should NOT be ready at 39 ticks gap");
    if (UartFrame_IsReady(&g_ctx)        != 0) FAIL("T1: IsReady should be 0 at 39 ticks");

    /* at 140 ticks (40 gap) — READY */
    if (UartFrame_Service(&g_ctx, 140UL) != 1) FAIL("T1: frame should be ready at 40 ticks gap");
    if (UartFrame_IsReady(&g_ctx)        != 1) FAIL("T1: IsReady should be 1");

    const uint16_t *data;
    uint16_t length;
    if (!UartFrame_GetReadyData(&g_ctx, &data, &length)) FAIL("T1: GetReadyData should succeed");
    if (length != 1U)                    FAIL("T1: length should be 1");
    if ((data[0] & 0xFFU) != 0xABU)     FAIL("T1: data[0] should be 0xAB");

    UartFrame_Consume(&g_ctx);
    if (UartFrame_IsReady(&g_ctx) != 0) FAIL("T1: IsReady should be 0 after consume");

    diag_check(1UL, 1UL, 0UL, 0UL, 0UL, 0U, "T1");
}

/* ==================================================================
 * Test 2: 64-byte exact frame
 * ================================================================== */
static void test_64byte_frame(void)
{
    UartFrame_Init(&g_ctx);

    uint32_t t = 0UL;
    uint16_t i;
    for (i = 0U; i < 64U; i++)
    {
        UartFrame_OnByte(&g_ctx, (uint16_t)(i & 0xFFU), t);
        t++;
    }

    if (UartFrame_Service(&g_ctx, t) != 0) FAIL("T2: should NOT be ready before gap");

    t += 40UL;
    if (UartFrame_Service(&g_ctx, t) != 1) FAIL("T2: should be ready after 40-ticks gap");

    const uint16_t *data;
    uint16_t length;
    if (!UartFrame_GetReadyData(&g_ctx, &data, &length)) FAIL("T2: GetReadyData");
    if (length != 64U) FAIL("T2: length should be 64");

    for (i = 0U; i < 64U; i++)
    {
        if ((data[i] & 0xFFU) != (i & 0xFFU))
        {
            printf("  T2 mismatch at byte %u\n", (unsigned)i);
            FAIL("T2: data mismatch");
            break;
        }
    }

    UartFrame_Consume(&g_ctx);
    diag_check(64UL, 1UL, 0UL, 0UL, 0UL, 0U, "T2");
}

/* ==================================================================
 * Test 3: 65-byte overlong frame
 * ================================================================== */
static void test_65byte_overlong(void)
{
    UartFrame_Init(&g_ctx);

    uint32_t t = 0UL;
    uint16_t i;
    for (i = 0U; i < 65U; i++)
    {
        UartFrame_OnByte(&g_ctx, (uint16_t)((i + 1) & 0xFFU), t);
        t++;
    }

    {
        UartFrameDiagnostics diag;
        UartFrame_GetDiagnostics(&g_ctx, &diag);
        if (diag.too_long_frames != 1UL) FAIL("T3: tooLongFrames should be 1");
    }

    if (UartFrame_Service(&g_ctx, t) != 0)      FAIL("T3: should not be ready while TOO_LONG");
    if (UartFrame_IsReady(&g_ctx)     != 0)      FAIL("T3: IsReady should be 0");

    /* After 40 ticks gap — reset to IDLE */
    t += 40UL;
    if (UartFrame_Service(&g_ctx, t) != 0) FAIL("T3: service should return 0 after gap");

    /* Now a new 3-byte frame should work */
    UartFrame_OnByte(&g_ctx, 0x10, t); t++;
    UartFrame_OnByte(&g_ctx, 0x20, t); t++;
    UartFrame_OnByte(&g_ctx, 0x30, t); t += 40UL;

    if (UartFrame_Service(&g_ctx, t) != 1) FAIL("T3: new frame after overlong should be ready");
    const uint16_t *data;
    uint16_t length;
    if (!UartFrame_GetReadyData(&g_ctx, &data, &length)) FAIL("T3: GetReadyData");
    if (length != 3U) FAIL("T3: new frame length should be 3");
    if ((data[0] & 0xFFU) != 0x10U) FAIL("T3: new frame data[0]");
    if ((data[1] & 0xFFU) != 0x20U) FAIL("T3: new frame data[1]");
    if ((data[2] & 0xFFU) != 0x30U) FAIL("T3: new frame data[2]");

    UartFrame_Consume(&g_ctx);
    diag_check(68UL, 1UL, 1UL, 0UL, 0UL, 0U, "T3");
}

/* ==================================================================
 * Test 4: boundary — 39 ticks no publish, 40 ticks publish
 * ================================================================== */
static void test_gap_boundary(void)
{
    UartFrame_Init(&g_ctx);

    UartFrame_OnByte(&g_ctx, 0x55, 0UL);

    if (UartFrame_Service(&g_ctx, 39UL) != 0) FAIL("T4: 39 ticks gap should NOT publish");
    if (UartFrame_Service(&g_ctx, 40UL) != 1) FAIL("T4: 40 ticks gap should publish");

    UartFrame_Consume(&g_ctx);
}

/* ==================================================================
 * Test 5: two consecutive frames
 * ================================================================== */
static void test_consecutive_frames(void)
{
    UartFrame_Init(&g_ctx);

    uint32_t t = 0UL;

    /* Frame 1: 3 bytes */
    UartFrame_OnByte(&g_ctx, 0xAA, t); t++;
    UartFrame_OnByte(&g_ctx, 0xBB, t); t++;
    UartFrame_OnByte(&g_ctx, 0xCC, t); t += 40UL;

    if (UartFrame_Service(&g_ctx, t) != 1) FAIL("T5: frame 1 should be ready");

    const uint16_t *d1;
    uint16_t len1;
    UartFrame_GetReadyData(&g_ctx, &d1, &len1);
    if (len1 != 3U) FAIL("T5: frame 1 length");
    uint16_t d1_copy[3] = { d1[0], d1[1], d1[2] };

    UartFrame_Consume(&g_ctx);

    /* Frame 2: 2 bytes */
    UartFrame_OnByte(&g_ctx, 0xDD, t); t++;
    UartFrame_OnByte(&g_ctx, 0xEE, t); t += 40UL;

    if (UartFrame_Service(&g_ctx, t) != 1) FAIL("T5: frame 2 should be ready");

    const uint16_t *d2;
    uint16_t len2;
    UartFrame_GetReadyData(&g_ctx, &d2, &len2);
    if (len2 != 2U) FAIL("T5: frame 2 length");

    if ((d1_copy[0] & 0xFFU) != 0xAAU) FAIL("T5: frame 1 data[0] after consume+new");
    if ((d1_copy[1] & 0xFFU) != 0xBBU) FAIL("T5: frame 1 data[1] after consume+new");
    if ((d1_copy[2] & 0xFFU) != 0xCCU) FAIL("T5: frame 1 data[2] after consume+new");

    if ((d2[0] & 0xFFU) != 0xDDU) FAIL("T5: frame 2 data[0]");
    if ((d2[1] & 0xFFU) != 0xEEU) FAIL("T5: frame 2 data[1]");

    UartFrame_Consume(&g_ctx);

    diag_check(5UL, 2UL, 0UL, 0UL, 0UL, 0U, "T5");
}

/* ==================================================================
 * Test 6: 32-bit tick wraparound
 * ================================================================== */
static void test_tick_wraparound(void)
{
    UartFrame_Init(&g_ctx);

    uint32_t lastRx = 0xFFFFFFFEUL;
    UartFrame_OnByte(&g_ctx, 0x77, lastRx);

    if (UartFrame_Service(&g_ctx, 37UL) != 0) FAIL("T6: should NOT be ready at gap=39 (37-0xFFFFFFFE)");
    if (UartFrame_Service(&g_ctx, 38UL) != 1) FAIL("T6: should be ready at gap=40 (38-0xFFFFFFFE)");

    const uint16_t *data;
    uint16_t length;
    UartFrame_GetReadyData(&g_ctx, &data, &length);
    if (length != 1U) FAIL("T6: length should be 1");
    if ((data[0] & 0xFFU) != 0x77U) FAIL("T6: data should be 0x77");
    UartFrame_Consume(&g_ctx);

    /* Byte received at tick 0xFFFFFFFF */
    UartFrame_OnByte(&g_ctx, 0x88, 0xFFFFFFFFUL);

    if (UartFrame_Service(&g_ctx, 38UL) != 0) FAIL("T6b: should NOT be ready at gap=39 (38-0xFFFFFFFF)");
    if (UartFrame_Service(&g_ctx, 39UL) != 1) FAIL("T6b: should be ready at gap=40 (39-0xFFFFFFFF)");
    UartFrame_GetReadyData(&g_ctx, &data, &length);
    if ((data[0] & 0xFFU) != 0x88U) FAIL("T6b: data should be 0x88");
    UartFrame_Consume(&g_ctx);
}

/* ==================================================================
 * Test 7: UART error discards candidate, recovery works
 * ================================================================== */
static void test_uart_error_recovery(void)
{
    UartFrame_Init(&g_ctx);

    uint32_t t = 0UL;

    UartFrame_OnByte(&g_ctx, 0x11, t); t++;
    UartFrame_OnByte(&g_ctx, 0x22, t); t++;

    UartFrame_OnError(&g_ctx, 0x04U, t);

    {
        UartFrameDiagnostics diag;
        UartFrame_GetDiagnostics(&g_ctx, &diag);
        if (diag.uart_errors  != 1UL)  FAIL("T7: uartErrors should be 1");
        if (diag.last_error   != 0x04U) FAIL("T7: lastError should be 0x04");
        if (diag.ready_frames != 0UL)  FAIL("T7: no ready frame from error");
        if (diag.rx_bytes     != 2UL)  FAIL("T7: rxBytes should be 2");
    }
    if (UartFrame_IsReady(&g_ctx) != 0) FAIL("T7: IsReady should be 0 after error");

    /* Recovery */
    UartFrame_OnByte(&g_ctx, 0x99, t); t += 40UL;
    if (UartFrame_Service(&g_ctx, t) != 1) FAIL("T7: recovery frame should be ready");
    const uint16_t *data;
    uint16_t length;
    UartFrame_GetReadyData(&g_ctx, &data, &length);
    if (length != 1U) FAIL("T7: recovery length should be 1");
    if ((data[0] & 0xFFU) != 0x99U) FAIL("T7: recovery data should be 0x99");
    UartFrame_Consume(&g_ctx);

    {
        UartFrameDiagnostics diag;
        UartFrame_GetDiagnostics(&g_ctx, &diag);
        if (diag.ready_frames != 1UL) FAIL("T7: readyFrames should be 1 after recovery");
    }
}

/* ==================================================================
 * Test 8: READY not consumed — new bytes do not overwrite
 * ================================================================== */
static void test_ready_not_consumed(void)
{
    UartFrame_Init(&g_ctx);

    uint32_t t = 0UL;

    /* Frame 1: 2 bytes → READY */
    UartFrame_OnByte(&g_ctx, 0x41, t); t++;
    UartFrame_OnByte(&g_ctx, 0x42, t); t += 40UL;

    if (UartFrame_Service(&g_ctx, t) != 1) FAIL("T8: frame 1 should be ready");

    const uint16_t *data;
    uint16_t length;
    UartFrame_GetReadyData(&g_ctx, &data, &length);
    if (length != 2U) FAIL("T8: frame 1 length");
    uint16_t d1_0 = data[0];
    uint16_t d1_1 = data[1];

    /* New bytes arrive while READY, not consumed */
    UartFrame_OnByte(&g_ctx, 0x51, t); t++;
    UartFrame_OnByte(&g_ctx, 0x52, t); t++;
    UartFrame_OnByte(&g_ctx, 0x53, t); t += 40UL;

    {
        UartFrameDiagnostics diag;
        UartFrame_GetDiagnostics(&g_ctx, &diag);
        if (diag.busy_drops != 3UL) FAIL("T8: busyDrops should be 3");
    }

    /* Frame 1 data unchanged */
    if (data[0] != d1_0) FAIL("T8: data[0] overwritten");
    if (data[1] != d1_1) FAIL("T8: data[1] overwritten");
    if (length != 2U) FAIL("T8: length should still be 2");

    {
        UartFrameDiagnostics diag;
        UartFrame_GetDiagnostics(&g_ctx, &diag);
        if (diag.ready_frames != 1UL) FAIL("T8: readyFrames should still be 1");
    }

    /* Consume and then receive a clean 2-byte frame */
    UartFrame_Consume(&g_ctx);
    UartFrame_OnByte(&g_ctx, 0x61, t); t++;
    UartFrame_OnByte(&g_ctx, 0x62, t); t += 40UL;

    if (UartFrame_Service(&g_ctx, t) != 1) FAIL("T8: frame 2 should be ready");
    UartFrame_GetReadyData(&g_ctx, &data, &length);
    if (length != 2U) FAIL("T8: frame 2 length");
    if ((data[0] & 0xFFU) != 0x61U) FAIL("T8: frame 2 data[0]");
    if ((data[1] & 0xFFU) != 0x62U) FAIL("T8: frame 2 data[1]");

    {
        UartFrameDiagnostics diag;
        UartFrame_GetDiagnostics(&g_ctx, &diag);
        if (diag.busy_drops != 3UL) FAIL("T8: busyDrops should still be 3");
    }
    UartFrame_Consume(&g_ctx);
}

/* ==================================================================
 * Test 9: Diagnostics snapshot does not modify values
 * ================================================================== */
static void test_diag_snapshot_idempotent(void)
{
    UartFrame_Init(&g_ctx);

    UartFrame_OnByte(&g_ctx, 0x01, 0UL);
    UartFrame_OnByte(&g_ctx, 0x02, 0UL);

    UartFrameDiagnostics snap1, snap2;
    UartFrame_GetDiagnostics(&g_ctx, &snap1);
    UartFrame_GetDiagnostics(&g_ctx, &snap2);

    if (snap1.rx_bytes != snap2.rx_bytes) FAIL("T9: rx_bytes should be idempotent");
    if (snap1.rx_bytes != 2UL) FAIL("T9: rx_bytes should be 2");
}

/* ==================================================================
 * Runner
 * ================================================================== */
int main(void)
{
    printf("=== uart_frame host tests ===\n");

    test_1byte_frame();
    test_64byte_frame();
    test_65byte_overlong();
    test_gap_boundary();
    test_consecutive_frames();
    test_tick_wraparound();
    test_uart_error_recovery();
    test_ready_not_consumed();
    test_diag_snapshot_idempotent();

    if (g_failures == 0)
    {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}

#endif /* !__TMS320C28XX__ */
