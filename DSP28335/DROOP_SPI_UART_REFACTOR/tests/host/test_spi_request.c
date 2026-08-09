#ifdef __TMS320C28XX__
static int _host_test_placeholder_spi;
#else

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../../firmware/services/spi_request.h"

static SpiRequestContext g_ctx;
static int g_failures = 0;

#define FAIL(msg) do { \
    printf("FAIL: %s\n", (msg)); fflush(stdout); \
    g_failures++; \
} while(0)

static void diag_reset(void)
{
    SpiRequest_Init(&g_ctx);
}

/* ---- Mock SPI callbacks ---- */
static uint16_t mock_tx_buf[64];
static uint16_t mock_tx_count;
static uint16_t mock_tx_prev;
static uint16_t mock_rx_count;
static uint16_t mock_rx_prev;
static uint16_t mock_rx_val;
static int mock_start_busy;
static int mock_complete_done;

static void mock_reset(void)
{
    memset(mock_tx_buf, 0, sizeof(mock_tx_buf));
    mock_tx_count      = 0U;
    mock_tx_prev       = 0U;
    mock_rx_count      = 0U;
    mock_rx_prev       = 0U;
    mock_rx_val        = 0xFFU;
    mock_start_busy    = 0;
    mock_complete_done = 0;
}

static int mock_startFn(uint16_t txByte)
{
    if (mock_start_busy) return 0;
    mock_tx_buf[mock_tx_count] = txByte;
    mock_tx_count++;
    mock_complete_done = 1;
    return 1;
}

static int mock_completeFn(uint16_t *rxByte)
{
    if (!mock_complete_done) return 0;
    *rxByte = mock_rx_val;
    mock_rx_count++;
    mock_complete_done = 0;
    return 1;
}

/* ==================================================================
 * Test 1: 64-byte request — MOSI matches input byte-for-byte.
 * ================================================================== */
static void test_64byte_request(void)
{
    diag_reset();
    mock_reset();

    static uint16_t input[64];
    uint16_t i;
    for (i = 0U; i < 64U; i++) input[i] = (uint16_t)((i * 3 + 7) & 0xFFU);

    uint32_t now = 1000UL;

    SpiRequest_Start(&g_ctx, input, 64U, now);
    if (SpiRequest_IsIdle(&g_ctx)) FAIL("T1: should not be idle after Start");

    uint32_t totalServiceCalls = 0UL;
    int state;

    for (;;)
    {
        mock_tx_prev = mock_tx_count;
        mock_rx_prev = mock_rx_count;

        state = SpiRequest_Service(&g_ctx, now, mock_startFn, mock_completeFn);
        totalServiceCalls++;

        uint16_t txDelta = mock_tx_count - mock_tx_prev;
        uint16_t rxDelta = mock_rx_count - mock_rx_prev;

        if (txDelta > 1U) FAIL("T1: >1 start per Service");
        if (rxDelta > 1U) FAIL("T1: >1 complete per Service");
        if (txDelta == 1U && rxDelta == 1U) FAIL("T1: start+complete same Service call");

        if (state == SPI_REQ_DONE || state == SPI_REQ_TIMEOUT) break;

        now++;
    }

    if (state != SPI_REQ_DONE) FAIL("T1: should finish as DONE");
    if (mock_tx_count != 64U)  FAIL("T1: 64 bytes not sent");

    for (i = 0U; i < 64U; i++)
    {
        if ((mock_tx_buf[i] & 0xFFU) != (input[i] & 0xFFU))
        {
            printf("  T1 MOSI mismatch at byte %u: tx=0x%02X input=0x%02X\n",
                   (unsigned)i, (unsigned)mock_tx_buf[i], (unsigned)input[i]);
            FAIL("T1: MOSI mismatch");
            break;
        }
    }

    if (totalServiceCalls < 64U) FAIL("T1: too few Service calls");

    {
        SpiRequestDiagnostics diag;
        SpiRequest_GetDiagnostics(&g_ctx, &diag);
        if (diag.req_frames   != 1UL)  FAIL("T1: reqFrames should be 1");
        if (diag.req_bytes    != 64UL) FAIL("T1: reqBytes should be 64");
        if (diag.miso_idle_ff != 64UL) FAIL("T1: misoIdleFF should be 64");
    }

    SpiRequest_Finish(&g_ctx);
}

/* ==================================================================
 * Test 2: 1-byte request — minimal frame.
 * ================================================================== */
static void test_1byte_request(void)
{
    diag_reset();
    mock_reset();

    static uint16_t input[] = { 0xABU };
    uint32_t now = 100UL;

    SpiRequest_Start(&g_ctx, input, 1U, now);

    int state = SpiRequest_Service(&g_ctx, now, mock_startFn, mock_completeFn);
    if (state != SPI_REQ_WAIT_DONE) FAIL("T2: first Service should start byte → WAIT_DONE");
    if (mock_tx_count != 1U) FAIL("T2: first byte should be started");

    state = SpiRequest_Service(&g_ctx, now + 1UL, mock_startFn, mock_completeFn);
    if (state != SPI_REQ_DONE) FAIL("T2: second Service should complete → DONE");
    if (mock_rx_count != 1U) FAIL("T2: byte should be completed");

    if (mock_tx_buf[0] != 0xABU) FAIL("T2: MOSI should be 0xAB");

    {
        SpiRequestDiagnostics diag;
        SpiRequest_GetDiagnostics(&g_ctx, &diag);
        if (diag.req_frames != 1UL) FAIL("T2: reqFrames should be 1");
        if (diag.req_bytes  != 1UL) FAIL("T2: reqBytes should be 1");
    }

    SpiRequest_Finish(&g_ctx);
}

/* ==================================================================
 * Test 3: 10-tick byte gap between consecutive bytes.
 * ================================================================== */
static void test_10tick_byte_gap(void)
{
    diag_reset();
    mock_reset();

    static uint16_t input[3] = { 0x10U, 0x20U, 0x30U };
    uint32_t now = 0UL;

    SpiRequest_Start(&g_ctx, input, 3U, now);

    int state = SpiRequest_Service(&g_ctx, now, mock_startFn, mock_completeFn);
    if (state != SPI_REQ_WAIT_DONE) FAIL("T3: byte 0 should start");

    state = SpiRequest_Service(&g_ctx, now + 1UL, mock_startFn, mock_completeFn);
    if (state != SPI_REQ_WAIT_GAP) FAIL("T3: after byte 0 complete should be WAIT_GAP");

    uint32_t t = now + 1UL + 9UL;
    state = SpiRequest_Service(&g_ctx, t, mock_startFn, mock_completeFn);
    if (state != SPI_REQ_WAIT_GAP) FAIL("T3: gap=9 should stay WAIT_GAP");
    if (mock_tx_count != 1U) FAIL("T3: byte 1 should NOT start at gap=9");

    t = now + 1UL + 10UL;
    state = SpiRequest_Service(&g_ctx, t, mock_startFn, mock_completeFn);
    if (state != SPI_REQ_WAIT_DONE) FAIL("T3: gap=10 should start byte 1");
    if (mock_tx_count != 2U) FAIL("T3: byte 1 should start at gap=10");
    if (mock_tx_buf[1] != 0x20U) FAIL("T3: byte 1 MOSI should be 0x20");

    state = SpiRequest_Service(&g_ctx, t + 1UL, mock_startFn, mock_completeFn);
    if (state != SPI_REQ_WAIT_GAP) FAIL("T3: after byte 1 complete → WAIT_GAP");

    t = t + 1UL + 9UL;
    state = SpiRequest_Service(&g_ctx, t, mock_startFn, mock_completeFn);
    if (state != SPI_REQ_WAIT_GAP) FAIL("T3: gap=9 after byte 1");

    t = t + 1UL;
    state = SpiRequest_Service(&g_ctx, t, mock_startFn, mock_completeFn);
    if (state != SPI_REQ_WAIT_DONE) FAIL("T3: gap=10 should start byte 2");
    if (mock_tx_buf[2] != 0x30U) FAIL("T3: byte 2 MOSI should be 0x30");

    state = SpiRequest_Service(&g_ctx, t + 1UL, mock_startFn, mock_completeFn);
    if (state != SPI_REQ_DONE) FAIL("T3: after byte 2 complete → DONE");
    if (mock_tx_count != 3U) FAIL("T3: tx count should be 3");
    if (mock_rx_count != 3U) FAIL("T3: rx count should be 3");

    SpiRequest_Finish(&g_ctx);
}

/* ==================================================================
 * Test 4: tick wraparound (32-bit unsigned).
 * ================================================================== */
static void test_tick_wraparound(void)
{
    diag_reset();
    mock_reset();

    static uint16_t input[2] = { 0x55U, 0xAAU };
    uint32_t now = 0xFFFFFFFEUL;
    SpiRequest_Start(&g_ctx, input, 2U, now);

    int state = SpiRequest_Service(&g_ctx, now, mock_startFn, mock_completeFn);
    if (state != SPI_REQ_WAIT_DONE) FAIL("T4: byte 0 should start at 0xFFFFFFFE");

    state = SpiRequest_Service(&g_ctx, 0xFFFFFFFFUL, mock_startFn, mock_completeFn);
    if (state != SPI_REQ_WAIT_GAP) FAIL("T4: byte 0 complete → WAIT_GAP");

    state = SpiRequest_Service(&g_ctx, 8UL, mock_startFn, mock_completeFn);
    if (state != SPI_REQ_WAIT_GAP) FAIL("T4: gap=9 after wrap should stay WAIT_GAP");

    state = SpiRequest_Service(&g_ctx, 9UL, mock_startFn, mock_completeFn);
    if (state != SPI_REQ_WAIT_DONE) FAIL("T4: gap=10 after wrap should start byte 1");
    if (mock_tx_buf[1] != 0xAAU) FAIL("T4: byte 1 MOSI should be 0xAA");

    state = SpiRequest_Service(&g_ctx, 10UL, mock_startFn, mock_completeFn);
    if (state != SPI_REQ_DONE) FAIL("T4: should be DONE after byte 1 complete");

    SpiRequest_Finish(&g_ctx);
}

/* ==================================================================
 * Test 5: busy rejection — startFn returns 0 → TIMEOUT.
 * ================================================================== */
static void test_busy_rejection(void)
{
    diag_reset();
    mock_reset();

    static uint16_t input[2] = { 0x11U, 0x22U };
    uint32_t now = 0UL;

    SpiRequest_Start(&g_ctx, input, 2U, now);
    if (SpiRequest_IsIdle(&g_ctx)) FAIL("T5: should not be idle");

    mock_start_busy = 1;

    int state = SpiRequest_Service(&g_ctx, now, mock_startFn, mock_completeFn);
    if (state != SPI_REQ_TIMEOUT) FAIL("T5: busy start should → TIMEOUT");

    {
        SpiRequestDiagnostics diag;
        SpiRequest_GetDiagnostics(&g_ctx, &diag);
        if (diag.start_failures != 1UL) FAIL("T5: startFailures should be 1");
        if (diag.last_error != SPI_ERR_START_FAIL) FAIL("T5: lastError should be START_FAIL");
    }

    SpiRequest_Finish(&g_ctx);
    if (!SpiRequest_IsIdle(&g_ctx)) FAIL("T5: should be idle after Finish");

    /* Clean request should succeed */
    mock_reset();
    diag_reset();
    mock_start_busy = 0;

    static uint16_t input2[] = { 0x99U };
    now = 100UL;
    SpiRequest_Start(&g_ctx, input2, 1U, now);
    state = SpiRequest_Service(&g_ctx, now, mock_startFn, mock_completeFn);
    if (state != SPI_REQ_WAIT_DONE) FAIL("T5: clean request byte should start");
    state = SpiRequest_Service(&g_ctx, now + 1UL, mock_startFn, mock_completeFn);
    if (state != SPI_REQ_DONE) FAIL("T5: clean request should complete");
    if (mock_tx_buf[0] != 0x99U) FAIL("T5: clean MOSI should be 0x99");

    SpiRequest_Finish(&g_ctx);
}

/* ==================================================================
 * Test 6: SPI timeout — byte doesn't complete → TIMEOUT, then recovery.
 * ================================================================== */
static void test_spi_timeout_recovery(void)
{
    diag_reset();
    mock_reset();

    static uint16_t input[2] = { 0x41U, 0x42U };
    uint32_t now = 0UL;

    SpiRequest_Start(&g_ctx, input, 2U, now);

    int state = SpiRequest_Service(&g_ctx, now, mock_startFn, mock_completeFn);
    if (state != SPI_REQ_WAIT_DONE) FAIL("T6: byte 0 should start");

    mock_complete_done = 0;

    now = 49UL;
    state = SpiRequest_Service(&g_ctx, now, mock_startFn, mock_completeFn);
    if (state != SPI_REQ_WAIT_DONE) FAIL("T6: should still be WAIT_DONE at tick 49");

    state = SpiRequest_Service(&g_ctx, 50UL, mock_startFn, mock_completeFn);
    if (state != SPI_REQ_TIMEOUT) FAIL("T6: should TIMEOUT at tick 50");

    {
        SpiRequestDiagnostics diag;
        SpiRequest_GetDiagnostics(&g_ctx, &diag);
        if (diag.timeouts   != 1UL) FAIL("T6: timeouts should be 1");
        if (diag.last_error != SPI_ERR_TIMEOUT) FAIL("T6: lastError should be TIMEOUT");
    }

    SpiRequest_Finish(&g_ctx);
    if (!SpiRequest_IsIdle(&g_ctx)) FAIL("T6: should be idle after timeout Finish");

    /* Recovery */
    mock_reset();
    diag_reset();

    static uint16_t input2[] = { 0x88U };
    now = 200UL;
    mock_complete_done = 0;

    SpiRequest_Start(&g_ctx, input2, 1U, now);
    state = SpiRequest_Service(&g_ctx, now, mock_startFn, mock_completeFn);
    if (state != SPI_REQ_WAIT_DONE) FAIL("T6: recovery byte should start");
    state = SpiRequest_Service(&g_ctx, now + 1UL, mock_startFn, mock_completeFn);
    if (state != SPI_REQ_DONE) FAIL("T6: recovery should complete");
    if (mock_tx_buf[0] != 0x88U) FAIL("T6: recovery MOSI should be 0x88");

    SpiRequest_Finish(&g_ctx);
}

/* ==================================================================
 * Test 7: MISO 0xFF counted as idle.
 * ================================================================== */
static void test_miso_0xFF_counting(void)
{
    diag_reset();
    mock_reset();

    static uint16_t input[4] = { 0x01U, 0x02U, 0x03U, 0x04U };
    uint32_t now = 0UL;

    mock_rx_val = 0xFFU;

    SpiRequest_Start(&g_ctx, input, 4U, now);
    int state;
    do {
        state = SpiRequest_Service(&g_ctx, now, mock_startFn, mock_completeFn);
        now++;
    } while (state != SPI_REQ_DONE && state != SPI_REQ_TIMEOUT);

    if (state != SPI_REQ_DONE) FAIL("T7: should be DONE");

    {
        SpiRequestDiagnostics diag;
        SpiRequest_GetDiagnostics(&g_ctx, &diag);
        if (diag.miso_idle_ff    != 4UL) FAIL("T7: misoIdleFF should be 4");
        if (diag.miso_unexpected != 0UL) FAIL("T7: misoUnexpected should be 0");
    }

    SpiRequest_Finish(&g_ctx);
}

/* ==================================================================
 * Test 8: MISO non-0xFF counted as unexpected.
 * ================================================================== */
static void test_miso_unexpected_counting(void)
{
    diag_reset();
    mock_reset();

    static uint16_t input[3] = { 0xA0U, 0xB0U, 0xC0U };
    uint32_t now = 0UL;

    mock_rx_val = 0x7EU;

    SpiRequest_Start(&g_ctx, input, 3U, now);
    int state;
    do {
        state = SpiRequest_Service(&g_ctx, now, mock_startFn, mock_completeFn);
        now++;
    } while (state != SPI_REQ_DONE && state != SPI_REQ_TIMEOUT);

    if (state != SPI_REQ_DONE) FAIL("T8: should be DONE");

    {
        SpiRequestDiagnostics diag;
        SpiRequest_GetDiagnostics(&g_ctx, &diag);
        if (diag.miso_idle_ff    != 0UL) FAIL("T8: misoIdleFF should be 0");
        if (diag.miso_unexpected != 3UL) FAIL("T8: misoUnexpected should be 3");
        if (diag.req_bytes       != 3UL) FAIL("T8: reqBytes should be 3");
    }

    SpiRequest_Finish(&g_ctx);
}

/* ==================================================================
 * Test 9: Start ignores when not IDLE.
 * ================================================================== */
static void test_start_ignores_when_busy(void)
{
    diag_reset();
    mock_reset();

    static uint16_t input1[2] = { 0x11U, 0x22U };
    static uint16_t input2[2] = { 0x33U, 0x44U };
    uint32_t now = 0UL;

    SpiRequest_Start(&g_ctx, input1, 2U, now);

    /* Try to start second request while first is active — should be ignored */
    SpiRequest_Start(&g_ctx, input2, 2U, now);
    if (SpiRequest_IsIdle(&g_ctx)) FAIL("T9: should not be idle");

    int state;
    do {
        state = SpiRequest_Service(&g_ctx, now, mock_startFn, mock_completeFn);
        now++;
    } while (state != SPI_REQ_DONE && state != SPI_REQ_TIMEOUT);

    if (mock_tx_buf[0] != 0x11U) FAIL("T9: byte 0 should be 0x11");
    if (mock_tx_buf[1] != 0x22U) FAIL("T9: byte 1 should be 0x22");
    if (mock_tx_count != 2U) FAIL("T9: should send exactly 2 bytes");

    SpiRequest_Finish(&g_ctx);
}

/* ==================================================================
 * Test 10: Zero-length or NULL data rejected at Start.
 * ================================================================== */
static void test_invalid_start(void)
{
    diag_reset();
    mock_reset();

    static uint16_t input[] = { 0x55U };
    uint32_t now = 100UL;

    SpiRequest_Start(&g_ctx, input, 0U, now);
    if (!SpiRequest_IsIdle(&g_ctx)) FAIL("T10: length 0 should stay IDLE");

    SpiRequest_Start(&g_ctx, ((const uint16_t *)0), 5U, now);
    if (!SpiRequest_IsIdle(&g_ctx)) FAIL("T10: NULL data should stay IDLE");
}

/* ==================================================================
 * Test 11: Two consecutive requests work after Finish.
 * ================================================================== */
static void test_consecutive_requests(void)
{
    diag_reset();
    mock_reset();

    static uint16_t input1[2] = { 0xAAU, 0xBBU };
    uint32_t now = 0UL;

    SpiRequest_Start(&g_ctx, input1, 2U, now);
    int state;
    do {
        state = SpiRequest_Service(&g_ctx, now, mock_startFn, mock_completeFn);
        now++;
    } while (state != SPI_REQ_DONE && state != SPI_REQ_TIMEOUT);

    if (state != SPI_REQ_DONE) FAIL("T11: req 1 should be DONE");
    if (mock_tx_buf[0] != 0xAAU) FAIL("T11: req 1 byte 0");
    if (mock_tx_buf[1] != 0xBBU) FAIL("T11: req 1 byte 1");

    {
        SpiRequestDiagnostics diag;
        SpiRequest_GetDiagnostics(&g_ctx, &diag);
        if (diag.req_frames != 1UL) FAIL("T11: reqFrames should be 1 after req 1");
    }

    SpiRequest_Finish(&g_ctx);
    if (!SpiRequest_IsIdle(&g_ctx)) FAIL("T11: should be idle after Finish");

    /* Request 2 */
    mock_reset();

    static uint16_t input2[3] = { 0xCCU, 0xDDU, 0xEEU };

    SpiRequest_Start(&g_ctx, input2, 3U, now);
    do {
        state = SpiRequest_Service(&g_ctx, now, mock_startFn, mock_completeFn);
        now++;
    } while (state != SPI_REQ_DONE && state != SPI_REQ_TIMEOUT);

    if (state != SPI_REQ_DONE) FAIL("T11: req 2 should be DONE");
    if (mock_tx_buf[0] != 0xCCU) FAIL("T11: req 2 byte 0");
    if (mock_tx_buf[1] != 0xDDU) FAIL("T11: req 2 byte 1");
    if (mock_tx_buf[2] != 0xEEU) FAIL("T11: req 2 byte 2");

    {
        SpiRequestDiagnostics diag;
        SpiRequest_GetDiagnostics(&g_ctx, &diag);
        if (diag.req_frames != 2UL) FAIL("T11: reqFrames should be 2 after req 2");
    }

    SpiRequest_Finish(&g_ctx);
}

/* ==================================================================
 * Test 12: MISO 0x00 — zero byte counted as unexpected.
 * ================================================================== */
static void test_miso_zero_byte(void)
{
    diag_reset();
    mock_reset();

    static uint16_t input[2] = { 0x01U, 0x02U };
    uint32_t now = 0UL;

    mock_rx_val = 0x00U;

    SpiRequest_Start(&g_ctx, input, 2U, now);
    int state;
    do {
        state = SpiRequest_Service(&g_ctx, now, mock_startFn, mock_completeFn);
        now++;
    } while (state != SPI_REQ_DONE && state != SPI_REQ_TIMEOUT);

    if (state != SPI_REQ_DONE) FAIL("T12: should be DONE");

    {
        SpiRequestDiagnostics diag;
        SpiRequest_GetDiagnostics(&g_ctx, &diag);
        if (diag.miso_idle_ff    != 0UL) FAIL("T12: misoIdleFF should be 0");
        if (diag.miso_unexpected != 2UL) FAIL("T12: misoUnexpected should be 2 for 0x00");
    }

    SpiRequest_Finish(&g_ctx);
}

/* ==================================================================
 * Test 13: Full 0x00–0xFF transparency.
 * ================================================================== */
static void test_full_byte_range(void)
{
    diag_reset();
    mock_reset();

    static uint16_t input[64];
    uint16_t i;
    for (i = 0U; i < 64U; i++) input[i] = i;
    uint32_t now = 0UL;

    SpiRequest_Start(&g_ctx, input, 64U, now);
    int state;
    do {
        state = SpiRequest_Service(&g_ctx, now, mock_startFn, mock_completeFn);
        now++;
    } while (state != SPI_REQ_DONE && state != SPI_REQ_TIMEOUT);

    if (state != SPI_REQ_DONE) FAIL("T13: should be DONE");

    for (i = 0U; i < 64U; i++)
    {
        if ((mock_tx_buf[i] & 0xFFU) != i)
        {
            printf("  T13 mismatch at byte %u: tx=0x%02X\n", (unsigned)i, (unsigned)mock_tx_buf[i]);
            FAIL("T13: byte range mismatch");
            break;
        }
    }

    SpiRequest_Finish(&g_ctx);
}

/* ==================================================================
 * Test 14: Diagnostics snapshot idempotent.
 * ================================================================== */
static void test_diag_snapshot_idempotent(void)
{
    diag_reset();
    mock_reset();

    static uint16_t input[1] = { 0x77U };
    SpiRequest_Start(&g_ctx, input, 1U, 100UL);

    SpiRequestDiagnostics snap1, snap2;
    SpiRequest_GetDiagnostics(&g_ctx, &snap1);
    SpiRequest_GetDiagnostics(&g_ctx, &snap2);

    if (snap1.req_frames != snap2.req_frames) FAIL("T14: snapshot idempotent");
    if (snap1.req_bytes != snap2.req_bytes) FAIL("T14: snapshot idempotent 2");

    SpiRequest_Finish(&g_ctx);
}

/* ==================================================================
 * Runner
 * ================================================================== */
int main(void)
{
    printf("=== spi_request host tests ===\n");

    test_64byte_request();
    test_1byte_request();
    test_10tick_byte_gap();
    test_tick_wraparound();
    test_busy_rejection();
    test_spi_timeout_recovery();
    test_miso_0xFF_counting();
    test_miso_unexpected_counting();
    test_start_ignores_when_busy();
    test_invalid_start();
    test_consecutive_requests();
    test_miso_zero_byte();
    test_full_byte_range();
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
