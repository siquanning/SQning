#include "firmware/app/sci_rx_queue.h"

#pragma DATA_SECTION(g_sci_rx_items, "comm_buffer")
static SciRxItem g_sci_rx_items[SCI_RX_QUEUE_BUF_SIZE];

void SciRxQueue_Init(SciRxQueue *queue)
{
    uint16_t i;
    for (i = 0U; i < SCI_RX_QUEUE_BUF_SIZE; i++)
    {
        g_sci_rx_items[i].data        = 0U;
        g_sci_rx_items[i].error_flags = 0U;
        g_sci_rx_items[i].tick        = 0UL;
    }
    queue->items          = g_sci_rx_items;
    queue->write_index    = 0U;
    queue->read_index     = 0U;
    queue->overflow_count = 0U;
}

int SciRxQueue_PushFromIsr(SciRxQueue *queue,
                           uint16_t data,
                           uint16_t error_flags,
                           uint32_t tick)
{
    uint16_t next_write = queue->write_index + 1U;

    if (next_write >= SCI_RX_QUEUE_BUF_SIZE)
    {
        next_write = 0U;
    }

    if (next_write == queue->read_index)
    {
        queue->overflow_count++;
        return 0;
    }

    queue->items[queue->write_index].data        = data;
    queue->items[queue->write_index].error_flags = error_flags;
    queue->items[queue->write_index].tick        = tick;

    queue->write_index = next_write;
    return 1;
}

int SciRxQueue_Pop(SciRxQueue *queue,
                   SciRxItem *item)
{
     if (queue->read_index == queue->write_index)
    {
        return 0;
    }

    item->data        = queue->items[queue->read_index].data;
    item->error_flags = queue->items[queue->read_index].error_flags;
    item->tick        = queue->items[queue->read_index].tick;

    {
        uint16_t next_read = queue->read_index + 1U;
        if (next_read >= SCI_RX_QUEUE_BUF_SIZE)
        {
            next_read = 0U;
        }
        queue->read_index = next_read;
    }

    return 1;
}

uint16_t SciRxQueue_GetOverflowCount(const SciRxQueue *queue)
{
    return queue->overflow_count;
}
