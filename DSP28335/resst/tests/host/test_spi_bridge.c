#ifdef __TMS320C28XX__
static int _host_test_placeholder_bridge;
#else

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../../firmware/services/spi_bridge.h"

static SpiBridgeContext g_ctx;
static int g_failures = 0;

#define FAIL(msg) do { \
    printf("FAIL: %s\n", (msg)); fflush(stdout); \
    g_failures++; \
} while(0)

/* ---- Mock SPI ---- */
static uint16_t mock_tx_buf[128];
static uint16_t mock_tx_count;
static uint16_t mock_rx_val;
static int mock_start_busy;
static int mock_complete_done;

static void mock_reset(void)
{
    memset(mock_tx_buf, 0, sizeof(mock_tx_buf));
    mock_tx_count      = 0U;
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
    mock_complete_done = 0;
    return 1;
}

static void bridge_service_until_idle(uint32_t *now)
{
    uint32_t prev_forwarded;
    int loop = 0;
    do {
        prev_forwarded = g_ctx.frames_forwarded;
        SpiBridge_Service(&g_ctx, *now, mock_startFn, mock_completeFn);
        (*now)++;
        loop++;
        if (loop > 2000) { FAIL("bridge_service_until_idle: exceeded 2000 iterations"); break; }
    } while (g_ctx.frames_forwarded == prev_forwarded && g_ctx.spi_timeouts == 0UL);
}

/* ==================================================================
 * Test 1: 1-byte frame through bridge
 * ================================================================== */
static void test_1byte_bridge(void)
{
    SpiBridge_Init(&g_ctx);
    mock_reset();

    uint32_t now = 100UL;
    SpiBridge_OnRxByte(&g_ctx, 0xAB, now);
    now += 40UL;

    bridge_service_until_idle(&now);

    if (mock_tx_count != 1U) FAIL("B1: should send 1 byte");
    if ((mock_tx_buf[0] & 0xFFU) != 0xABU) FAIL("B1: byte mismatch");

    UartFrameDiagnostics diag;
    SpiBridge_GetUartDiagnostics(&g_ctx, &diag);
    if (diag.rx_bytes != 1UL) FAIL("B1: rxBytes should be 1");
    if (diag.ready_frames != 1UL) FAIL("B1: readyFrames should be 1");
    if (g_ctx.frames_forwarded != 1UL) FAIL("B1: frames_forwarded should be 1");
}

/* ==================================================================
 * Test 2: 64-byte frame through bridge
 * ================================================================== */
static void test_64byte_bridge(void)
{
    SpiBridge_Init(&g_ctx);
    mock_reset();

    uint32_t now = 0UL;
    uint16_t i;
    for (i = 0U; i < 64U; i++)
    {
        SpiBridge_OnRxByte(&g_ctx, (uint16_t)(i & 0xFFU), now);
        now++;
    }
    now += 40UL;

    bridge_service_until_idle(&now);

    if (mock_tx_count != 64U) FAIL("B2: should send 64 bytes");
    for (i = 0U; i < 64U; i++)
    {
        if ((mock_tx_buf[i] & 0xFFU) != (i & 0xFFU))
        {
            printf("  B2 mismatch at byte %u\n", (unsigned)i);
            FAIL("B2: data mismatch");
            break;
        }
    }
    if (g_ctx.frames_forwarded != 1UL) FAIL("B2: frames_forwarded should be 1");
}

/* ==================================================================
 * Test 3: 65-byte overlong — no SPI forward
 * ================================================================== */
static void test_65byte_overlong(void)
{
    SpiBridge_Init(&g_ctx);
    mock_reset();

    uint32_t now = 0UL;
    uint16_t i;
    for (i = 0U; i < 65U; i++)
    {
        SpiBridge_OnRxByte(&g_ctx, (uint16_t)i, now);
        now++;
    }

    SpiBridge_Service(&g_ctx, now, mock_startFn, mock_completeFn);

    if (mock_tx_count != 0U) FAIL("B3: should NOT send any SPI bytes");
    if (g_ctx.frames_forwarded != 0UL) FAIL("B3: frames_forwarded should be 0");

    {
        UartFrameDiagnostics diag;
        SpiBridge_GetUartDiagnostics(&g_ctx, &diag);
        if (diag.too_long_frames != 1UL) FAIL("B3: tooLongFrames should be 1");
    }

    /* After gap, new frame should work */
    now += 40UL;
    SpiBridge_Service(&g_ctx, now, mock_startFn, mock_completeFn);

    SpiBridge_OnRxByte(&g_ctx, 0x77, now); now += 40UL;
    bridge_service_until_idle(&now);

    if (mock_tx_count != 1U) FAIL("B3: recovery frame should be forwarded");
    if ((mock_tx_buf[0] & 0xFFU) != 0x77U) FAIL("B3: recovery byte mismatch");
    if (g_ctx.frames_forwarded != 1UL) FAIL("B3: frames_forwarded should be 1 after recovery");
}

/* ==================================================================
 * Test 4: READY frame not overwritten during SPI
 * ================================================================== */
static void test_ready_protected_during_spi(void)
{
    SpiBridge_Init(&g_ctx);
    mock_reset();

    uint32_t now = 0UL;

    /* Frame 1: 3 bytes */
    SpiBridge_OnRxByte(&g_ctx, 0x10, now); now++;
    SpiBridge_OnRxByte(&g_ctx, 0x20, now); now++;
    SpiBridge_OnRxByte(&g_ctx, 0x30, now); now += 40UL;

    /* Start SPI (first byte gets sent) */
    SpiBridge_Service(&g_ctx, now, mock_startFn, mock_completeFn);
    mock_complete_done = 1; /* Make byte complete next round */

    /* New bytes arrive while SPI is busy */
    SpiBridge_OnRxByte(&g_ctx, 0x99, now); now++;
    SpiBridge_OnRxByte(&g_ctx, 0x88, now); now++;
    SpiBridge_Service(&g_ctx, now, mock_startFn, mock_completeFn);

    /* Finish SPI the rest of the way */
    now++;
    bridge_service_until_idle(&now);

    /* Verify first 3 bytes sent, NOT the new ones */
    if (mock_tx_count != 3U) FAIL("B4: should send 3 bytes from frame 1");
    if ((mock_tx_buf[0] & 0xFFU) != 0x10U) FAIL("B4: byte 0 mismatch");
    if ((mock_tx_buf[1] & 0xFFU) != 0x20U) FAIL("B4: byte 1 mismatch");
    if ((mock_tx_buf[2] & 0xFFU) != 0x30U) FAIL("B4: byte 2 mismatch");

    {
        UartFrameDiagnostics diag;
        SpiBridge_GetUartDiagnostics(&g_ctx, &diag);
        if (diag.busy_drops != 2UL) FAIL("B4: busyDrops should be 2");
    }

    if (g_ctx.frames_forwarded != 1UL) FAIL("B4: frames_forwarded should be 1");
}

/* ==================================================================
 * Test 5: SPI timeout — frame consumed
 * ================================================================== */
static void test_spi_timeout(void)
{
    SpiBridge_Init(&g_ctx);
    mock_reset();

    uint32_t now = 0UL;

    SpiBridge_OnRxByte(&g_ctx, 0x55, now);

    /* Make SPI never complete */
    mock_complete_done = 0;

    /* Frame gap → READY → SPI started as WAIT_DONE */
    now += 40UL;
    SpiBridge_Service(&g_ctx, now, mock_startFn, mock_completeFn);

    /* Advance past timeout (50 ticks after start) */
    now += 50UL;
    SpiBridge_Service(&g_ctx, now, mock_startFn, mock_completeFn);

    if (g_ctx.spi_timeouts != 1UL) FAIL("B5: spi_timeouts should be 1");
    if (g_ctx.frames_forwarded != 0UL) FAIL("B5: frames_forwarded should be 0");
}

/* ==================================================================
 * Test 6: Tick wraparound — frame gap
 * ================================================================== */
static void test_tick_wraparound(void)
{
    SpiBridge_Init(&g_ctx);
    mock_reset();

    uint32_t now = 0xFFFFFFFEUL;

    SpiBridge_OnRxByte(&g_ctx, 0x33, now);

    /* gap = 37 - 0xFFFFFFFE = 39 (< 40) → NOT ready */
    SpiBridge_Service(&g_ctx, 37UL, mock_startFn, mock_completeFn);
    if (mock_tx_count != 0U) FAIL("B6: should not start SPI at gap=39");

    /* gap = 38 - 0xFFFFFFFE = 40 → READY */
    SpiBridge_Service(&g_ctx, 38UL, mock_startFn, mock_completeFn);

    mock_complete_done = 1;
    bridge_service_until_idle(&now);

    if (mock_tx_count != 1U) FAIL("B6: should send 1 byte after gap=40");
    if ((mock_tx_buf[0] & 0xFFU) != 0x33U) FAIL("B6: byte mismatch");
}

/* ==================================================================
 * Test 7: "ISR boundary" — new byte arrives between Service calls
 * ================================================================== */
static void test_isr_boundary(void)
{
    SpiBridge_Init(&g_ctx);
    mock_reset();

    uint32_t now = 0UL;

    /* Byte 1 arrives at t=0 */
    SpiBridge_OnRxByte(&g_ctx, 0x41, now);

    /* Service at t=35 — gap=35, <40 — NOT ready */
    SpiBridge_Service(&g_ctx, 35UL, mock_startFn, mock_completeFn);
    if (mock_tx_count != 0U) FAIL("B7: NOT ready at gap=35");

    /* Byte 2 arrives at t=36 — extends the frame */
    SpiBridge_OnRxByte(&g_ctx, 0x42, 36UL);

    /* Service at t=39 — gap=3 from last byte — NOT ready */
    SpiBridge_Service(&g_ctx, 39UL, mock_startFn, mock_completeFn);
    if (mock_tx_count != 0U) FAIL("B7: still receiving at t=39");

    /* Service at t=76 — gap=40 from last byte → READY */
    now = 76UL;
    SpiBridge_Service(&g_ctx, now, mock_startFn, mock_completeFn);
    mock_complete_done = 1;
    bridge_service_until_idle(&now);

    if (mock_tx_count != 2U) FAIL("B7: should send 2 bytes");
    if ((mock_tx_buf[0] & 0xFFU) != 0x41U) FAIL("B7: byte 0 mismatch");
    if ((mock_tx_buf[1] & 0xFFU) != 0x42U) FAIL("B7: byte 1 mismatch");
}

/* ==================================================================
 * Test 8: Two consecutive frames
 * ================================================================== */
static void test_consecutive_frames(void)
{
    SpiBridge_Init(&g_ctx);
    mock_reset();

    uint32_t now = 0UL;

    /* Frame 1 */
    SpiBridge_OnRxByte(&g_ctx, 0xA1, now); now++;
    SpiBridge_OnRxByte(&g_ctx, 0xA2, now); now += 40UL;
    bridge_service_until_idle(&now);

    if (g_ctx.frames_forwarded != 1UL) FAIL("B8: frames_forwarded should be 1 after frame 1");
    if (mock_tx_count != 2U) FAIL("B8: frame 1: 2 bytes");

    /* Frame 2 */
    SpiBridge_OnRxByte(&g_ctx, 0xB1, now); now++;
    SpiBridge_OnRxByte(&g_ctx, 0xB2, now); now++;
    SpiBridge_OnRxByte(&g_ctx, 0xB3, now); now += 40UL;
    bridge_service_until_idle(&now);

    if (g_ctx.frames_forwarded != 2UL) FAIL("B8: frames_forwarded should be 2 after frame 2");
    if (mock_tx_count != 5U) FAIL("B8: total bytes should be 5");
    if ((mock_tx_buf[2] & 0xFFU) != 0xB1U) FAIL("B8: frame 2 byte 0 mismatch");
}

/* ==================================================================
 * Test 9: UART error discards candidate
 * ================================================================== */
static void test_uart_error(void)
{
    SpiBridge_Init(&g_ctx);
    mock_reset();

    uint32_t now = 0UL;

    SpiBridge_OnRxByte(&g_ctx, 0x11, now); now++;
    SpiBridge_OnRxByte(&g_ctx, 0x22, now); now++;

    SpiBridge_OnRxError(&g_ctx, 0x04U, now);

    SpiBridge_Service(&g_ctx, now + 40UL, mock_startFn, mock_completeFn);
    if (mock_tx_count != 0U) FAIL("B9: should not forward after error");

    {
        UartFrameDiagnostics diag;
        SpiBridge_GetUartDiagnostics(&g_ctx, &diag);
        if (diag.uart_errors != 1UL) FAIL("B9: uartErrors should be 1");
    }

    /* Recovery */
    SpiBridge_OnRxByte(&g_ctx, 0x99, now); now += 40UL;
    bridge_service_until_idle(&now);

    if (g_ctx.frames_forwarded != 1UL) FAIL("B9: recovery frame should be forwarded");
    if ((mock_tx_buf[0] & 0xFFU) != 0x99U) FAIL("B9: recovery byte mismatch");
}

/* ==================================================================
 * Test 10: SPI start failure → TIMEOUT
 * ================================================================== */
static void test_spi_start_failure(void)
{
    SpiBridge_Init(&g_ctx);
    mock_reset();
    mock_start_busy = 1;

    uint32_t now = 100UL;
    SpiBridge_OnRxByte(&g_ctx, 0x77, now);
    now += 40UL;

    SpiBridge_Service(&g_ctx, now, mock_startFn, mock_completeFn);

    if (g_ctx.spi_timeouts != 1UL) FAIL("B10: spi_timeouts should be 1");
    if (g_ctx.frames_forwarded != 0UL) FAIL("B10: frames_forwarded should be 0");

    {
        SpiRequestDiagnostics diag;
        SpiBridge_GetSpiDiagnostics(&g_ctx, &diag);
        if (diag.start_failures != 1UL) FAIL("B10: startFailures should be 1");
    }
}

/* ==================================================================
 * Runner
 * ================================================================== */
int main(void)
{
    printf("=== spi_bridge host tests ===\n");

    test_1byte_bridge();
    test_64byte_bridge();
    test_65byte_overlong();
    test_ready_protected_during_spi();
    test_spi_timeout();
    test_tick_wraparound();
    test_isr_boundary();
    test_consecutive_frames();
    test_uart_error();
    test_spi_start_failure();

    if (g_failures == 0)
    {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}

#endif /* !__TMS320C28XX__ */
