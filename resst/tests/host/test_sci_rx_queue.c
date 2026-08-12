#ifdef __TMS320C28XX__
static int _host_test_placeholder_queue;
#else

#include <stdio.h>
#include <stdint.h>
#include "../../firmware/app/sci_rx_queue.h"

static int g_failures = 0;

#define FAIL(msg) do { \
    printf("FAIL: %s\n", (msg)); fflush(stdout); \
    g_failures++; \
} while(0)

/* ---- Test 1: basic FIFO order ---- */
static void test_fifo_order(void)
{
    SciRxQueue queue;
    SciRxQueue_Init(&queue);

    uint16_t i;
    for (i = 0U; i < 10U; i++)
    {
        if (!SciRxQueue_PushFromIsr(&queue, i, 0U, 100UL + (uint32_t)i))
            FAIL("T1: push should succeed");
    }

    for (i = 0U; i < 10U; i++)
    {
        SciRxItem item;
        if (!SciRxQueue_Pop(&queue, &item))
            FAIL("T1: pop should succeed");
        if (item.data != i)
            FAIL("T1: data order mismatch");
        if (item.error_flags != 0U)
            FAIL("T1: error_flags should be 0");
        if (item.tick != 100UL + (uint32_t)i)
            FAIL("T1: tick mismatch");
    }

    /* Queue should now be empty */
    {
        SciRxItem item;
        if (SciRxQueue_Pop(&queue, &item))
            FAIL("T1: pop on empty should fail");
    }
}

/* ---- Test 2: empty queue ---- */
static void test_empty_queue(void)
{
    SciRxQueue queue;
    SciRxQueue_Init(&queue);

    SciRxItem item;
    if (SciRxQueue_Pop(&queue, &item))
        FAIL("T2: pop on empty queue should return 0");

    if (SciRxQueue_GetOverflowCount(&queue) != 0U)
        FAIL("T2: overflow should be 0");
}

/* ---- Test 3: full queue + overflow count ---- */
static void test_full_and_overflow(void)
{
    SciRxQueue queue;
    SciRxQueue_Init(&queue);

    uint16_t i;
    /* Fill to effective capacity (128 items) */
    for (i = 0U; i < SCI_RX_QUEUE_CAPACITY; i++)
    {
        if (!SciRxQueue_PushFromIsr(&queue, i, 0U, 0UL))
        {
            printf("  T3: push failed at i=%u\n", (unsigned)i);
            FAIL("T3: push should succeed before full");
            return;
        }
    }

    /* Next push should fail (queue full) */
    if (SciRxQueue_PushFromIsr(&queue, 0xFFU, 0U, 0UL))
        FAIL("T3: push on full queue should return 0");

    if (SciRxQueue_GetOverflowCount(&queue) != 1U)
        FAIL("T3: overflow count should be 1");

    /* One more — overflow increments again */
    if (SciRxQueue_PushFromIsr(&queue, 0xFEU, 0U, 0UL))
        FAIL("T3: second push on full should also fail");
    if (SciRxQueue_GetOverflowCount(&queue) != 2U)
        FAIL("T3: overflow count should be 2");

    /* Pop one, then push should succeed */
    SciRxItem item;
    if (!SciRxQueue_Pop(&queue, &item))
        FAIL("T3: pop should succeed");
    if (item.data != 0U)
        FAIL("T3: first byte should be 0");

    if (!SciRxQueue_PushFromIsr(&queue, 0xAAU, 0U, 0UL))
        FAIL("T3: push after pop should succeed");
    if (SciRxQueue_GetOverflowCount(&queue) != 2U)
        FAIL("T3: overflow count should still be 2");

    /* Pop remaining 127 items, then verify last is 0xAA */
    for (i = 1U; i < SCI_RX_QUEUE_CAPACITY; i++)
    {
        if (!SciRxQueue_Pop(&queue, &item))
        {
            printf("  T3: pop failed at i=%u\n", (unsigned)i);
            FAIL("T3: pop should succeed");
            return;
        }
        if (item.data != i)
        {
            printf("  T3: data mismatch at i=%u, got=%u\n", (unsigned)i, (unsigned)item.data);
            FAIL("T3: data order mismatch after overflow");
            return;
        }
    }

    /* Last item should be 0xAA */
    if (!SciRxQueue_Pop(&queue, &item))
        FAIL("T3: last pop should succeed");
    if (item.data != 0xAAU)
        FAIL("T3: last byte should be 0xAA");

    /* Queue should be empty */
    if (SciRxQueue_Pop(&queue, &item))
        FAIL("T3: queue should be empty after pop all");
}

/* ---- Test 4: wraparound ---- */
static void test_wraparound(void)
{
    SciRxQueue queue;
    SciRxQueue_Init(&queue);

    /* Force indices near end of internal buffer (129 slots) */
    queue.write_index = SCI_RX_QUEUE_BUF_SIZE - 2U;
    queue.read_index  = SCI_RX_QUEUE_BUF_SIZE - 2U;

    /* Push two items — second should wrap write_index to 0 */
    if (!SciRxQueue_PushFromIsr(&queue, 0x10U, 0U, 1UL))
        FAIL("T4: push 1 should succeed");
    if (queue.write_index != SCI_RX_QUEUE_BUF_SIZE - 1U)
        FAIL("T4: write_index should be BUF_SIZE-1 after push 1");

    if (!SciRxQueue_PushFromIsr(&queue, 0x20U, 0U, 2UL))
        FAIL("T4: push 2 should succeed");
    if (queue.write_index != 0U)
        FAIL("T4: write_index should wrap to 0 after push 2");

    /* Pop both */
    SciRxItem item;
    if (!SciRxQueue_Pop(&queue, &item))
        FAIL("T4: pop 1 should succeed");
    if (item.data != 0x10U)
        FAIL("T4: item 1 data mismatch");
    if (queue.read_index != SCI_RX_QUEUE_BUF_SIZE - 1U)
        FAIL("T4: read_index should be BUF_SIZE-1 after pop 1");

    if (!SciRxQueue_Pop(&queue, &item))
        FAIL("T4: pop 2 should succeed");
    if (item.data != 0x20U)
        FAIL("T4: item 2 data mismatch");
    if (queue.read_index != 0U)
        FAIL("T4: read_index should wrap to 0 after pop 2");

    /* Queue should be empty */
    if (SciRxQueue_Pop(&queue, &item))
        FAIL("T4: queue should be empty");
}

/* ---- Test 5: error events interleaved with byte events ---- */
static void test_error_and_byte_ordering(void)
{
    SciRxQueue queue;
    SciRxQueue_Init(&queue);

    /* Simulate: byte, byte, error, byte */
    SciRxQueue_PushFromIsr(&queue, 0x41U, 0U, 100UL);
    SciRxQueue_PushFromIsr(&queue, 0x42U, 0U, 100UL);
    SciRxQueue_PushFromIsr(&queue, 0U, 0x04U, 100UL);  /* error event */
    SciRxQueue_PushFromIsr(&queue, 0x43U, 0U, 101UL);

    SciRxItem item;

    /* Byte 1 */
    if (!SciRxQueue_Pop(&queue, &item)) FAIL("T5: pop 1");
    if (item.data != 0x41U || item.error_flags != 0U) FAIL("T5: item 1 mismatch");

    /* Byte 2 */
    if (!SciRxQueue_Pop(&queue, &item)) FAIL("T5: pop 2");
    if (item.data != 0x42U || item.error_flags != 0U) FAIL("T5: item 2 mismatch");

    /* Error */
    if (!SciRxQueue_Pop(&queue, &item)) FAIL("T5: pop 3 (error)");
    if (item.error_flags != 0x04U) FAIL("T5: error item mismatch");

    /* Byte 3 */
    if (!SciRxQueue_Pop(&queue, &item)) FAIL("T5: pop 4");
    if (item.data != 0x43U || item.error_flags != 0U) FAIL("T5: item 4 mismatch");

    if (SciRxQueue_Pop(&queue, &item))
        FAIL("T5: queue should be empty");
}

/* ---- Test 6: single-producer single-consumer ownership ---- */
static void test_spsc_ownership(void)
{
    SciRxQueue queue;
    SciRxQueue_Init(&queue);

    /* ISR pushes 5 items */
    uint16_t i;
    for (i = 0U; i < 5U; i++)
        SciRxQueue_PushFromIsr(&queue, i, 0U, 0UL);

    /* Consumer reads one */
    SciRxItem item;
    SciRxQueue_Pop(&queue, &item);

    /* ISR pushes more (write_index now ahead of read_index by more) */
    for (i = 5U; i < 10U; i++)
        SciRxQueue_PushFromIsr(&queue, i, 0U, 0UL);

    /* Read all remaining */
    for (i = 1U; i < 10U; i++)
    {
        if (!SciRxQueue_Pop(&queue, &item))
        {
            printf("  T6: pop failed at i=%u\n", (unsigned)i);
            FAIL("T6: pop should succeed");
            return;
        }
        if (item.data != i)
        {
            printf("  T6: data mismatch at i=%u, got=%u\n", (unsigned)i, (unsigned)item.data);
            FAIL("T6: data order mismatch");
            return;
        }
    }

    if (SciRxQueue_Pop(&queue, &item))
        FAIL("T6: queue should be empty");
}

/* ---- Runner ---- */
int main(void)
{
    printf("=== sci_rx_queue host tests ===\n");

    test_fifo_order();
    test_empty_queue();
    test_full_and_overflow();
    test_wraparound();
    test_error_and_byte_ordering();
    test_spsc_ownership();

    if (g_failures == 0)
    {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}

#endif /* !__TMS320C28XX__ */
