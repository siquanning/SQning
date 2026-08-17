/* Created by Siquanning */
#include "uart_frame.h"

void UartFrame_Init(UartFrameContext *context)
{
    uint16_t i;
    for (i = 0U; i < UART_FRAME_CAPACITY; i++)
    {
        context->buffer[i] = 0U;
    }
    context->length           = 0U;
    context->state            = FRAME_IDLE;
    context->last_rx_tick     = 0UL;
    context->rx_bytes         = 0UL;
    context->ready_frames     = 0UL;
    context->too_long_frames  = 0UL;
    context->busy_drops       = 0UL;
    context->uart_errors      = 0UL;
    context->last_error       = 0U;
}

void UartFrame_OnByte(UartFrameContext *context,
                      uint16_t data,
                      uint32_t tick)
{
    context->rx_bytes++;
    context->last_rx_tick = tick;

    if (context->state == FRAME_READY)
    {
        context->busy_drops++;
        return;
    }

    if (context->state == FRAME_TOO_LONG)
    {
        return;
    }

    if (context->length >= UART_FRAME_CAPACITY)
    {
        context->length = 0U;
        context->state  = FRAME_TOO_LONG;
        context->too_long_frames++;
        return;
    }

    context->buffer[context->length] = data;
    context->length++;
    if (context->state == FRAME_IDLE)
    {
        context->state = FRAME_RECEIVING;
    }
}

void UartFrame_OnError(UartFrameContext *context,
                       uint16_t error_flags,
                       uint32_t tick)
{
    (void)tick;
    context->uart_errors++;
    context->last_error  = error_flags;
    context->length      = 0U;
    context->state       = FRAME_IDLE;
}

int UartFrame_Service(UartFrameContext *context,
                      uint32_t tick)
{
    uint32_t gap;

    if (context->state == FRAME_TOO_LONG)
    {
        gap = tick - context->last_rx_tick;
        if (gap >= UART_FRAME_GAP_TICKS)
        {
            context->state  = FRAME_IDLE;
            context->length = 0U;
        }
        return 0;
    }

    if (context->state != FRAME_RECEIVING)
    {
        return 0;
    }

    if (context->length == 0U)
    {
        return 0;
    }

    gap = tick - context->last_rx_tick;
    if (gap < UART_FRAME_GAP_TICKS)
    {
        return 0;
    }

    context->state = FRAME_READY;
    context->ready_frames++;
    return 1;
}

int UartFrame_IsReady(const UartFrameContext *context)
{
    return (context->state == FRAME_READY) ? 1 : 0;
}

int UartFrame_GetReadyData(const UartFrameContext *context,
                           const uint16_t **data,
                           uint16_t *length)
{
    if (context->state != FRAME_READY)
    {
        return 0;
    }
    *data   = context->buffer;
    *length = context->length;
    return 1;
}

void UartFrame_Consume(UartFrameContext *context)
{
    context->length = 0U;
    context->state  = FRAME_IDLE;
}

void UartFrame_GetDiagnostics(const UartFrameContext *context,
                              UartFrameDiagnostics *snapshot)
{
    snapshot->rx_bytes        = context->rx_bytes;
    snapshot->ready_frames    = context->ready_frames;
    snapshot->too_long_frames = context->too_long_frames;
    snapshot->busy_drops      = context->busy_drops;
    snapshot->uart_errors     = context->uart_errors;
    snapshot->last_error      = context->last_error;
}
