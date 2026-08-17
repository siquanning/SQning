/* Created by Siquanning */
#ifndef SCI_RX_QUEUE_H
#define SCI_RX_QUEUE_H

#include <stdint.h>

/* Effective storage: 128 items.
 * Buffer allocates 129 slots — the extra slot resolves the
 * write_index==read_index ambiguity in the "reserve one empty slot"
 * full/empty check. */
#define SCI_RX_QUEUE_CAPACITY  128U
#define SCI_RX_QUEUE_BUF_SIZE  (SCI_RX_QUEUE_CAPACITY + 1U)

typedef struct
{
    uint16_t data;
    uint16_t error_flags;
    uint32_t tick;
} SciRxItem;

typedef struct
{
    SciRxItem          *items;
    volatile uint16_t   write_index;
    volatile uint16_t   read_index;
    volatile uint16_t   overflow_count;
} SciRxQueue;

void SciRxQueue_Init(SciRxQueue *queue);

int SciRxQueue_PushFromIsr(SciRxQueue *queue,
                           uint16_t data,
                           uint16_t error_flags,
                           uint32_t tick);

int SciRxQueue_Pop(SciRxQueue *queue,
                   SciRxItem *item);

uint16_t SciRxQueue_GetOverflowCount(const SciRxQueue *queue);

#endif
