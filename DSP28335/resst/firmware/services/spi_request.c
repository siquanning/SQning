#include "spi_request.h"
#include "config/comm_config.h"

void SpiRequest_Init(SpiRequestContext *context)
{
    context->data             = (const uint16_t *)0;
    context->length           = 0U;
    context->index            = 0U;
    context->state            = SPI_REQ_IDLE;
    context->next_action_tick = 0UL;
    context->byte_started_tick = 0UL;

    context->req_frames       = 0UL;
    context->req_bytes        = 0UL;
    context->miso_idle_ff     = 0UL;
    context->miso_unexpected  = 0UL;
    context->start_failures   = 0UL;
    context->timeouts         = 0UL;
    context->last_error       = SPI_ERR_NONE;
}

int SpiRequest_IsIdle(const SpiRequestContext *context)
{
    return (context->state == SPI_REQ_IDLE) ? 1 : 0;
}

void SpiRequest_Start(SpiRequestContext *context,
                      const uint16_t *data,
                      uint16_t length,
                      uint32_t tick)
{
    if (context->state != SPI_REQ_IDLE)
    {
        return;
    }
    if (length == 0U || data == ((const uint16_t *)0))
    {
        return;
    }

    context->data             = data;
    context->length           = length;
    context->index            = 0U;
    context->next_action_tick = tick - SPI_BYTE_GAP_TICKS;
    context->state            = SPI_REQ_WAIT_GAP;
}

int SpiRequest_Service(SpiRequestContext *context,
                       uint32_t tick,
                       SpiStartFn startFn,
                       SpiCompleteFn completeFn)
{
    if (context->state == SPI_REQ_WAIT_GAP)
    {
        uint32_t gap = tick - context->next_action_tick;
        if (gap < SPI_BYTE_GAP_TICKS)
        {
            return SPI_REQ_WAIT_GAP;
        }

        {
            uint16_t txByte = context->data[context->index] & 0xFFU;
            if (startFn(txByte) == 0)
            {
                context->start_failures++;
                context->last_error = SPI_ERR_START_FAIL;
                context->state = SPI_REQ_TIMEOUT;
                return SPI_REQ_TIMEOUT;
            }
        }

        context->byte_started_tick = tick;
        context->state = SPI_REQ_WAIT_DONE;
        return SPI_REQ_WAIT_DONE;
    }

    if (context->state == SPI_REQ_WAIT_DONE)
    {
        uint32_t elapsed = tick - context->byte_started_tick;
        if (elapsed >= SPI_BYTE_TIMEOUT_TICKS)
        {
            context->timeouts++;
            context->last_error = SPI_ERR_TIMEOUT;
            context->state = SPI_REQ_TIMEOUT;
            return SPI_REQ_TIMEOUT;
        }

        {
            uint16_t rxByte;
            if (completeFn(&rxByte) == 0)
            {
                return SPI_REQ_WAIT_DONE;
            }

            if (rxByte == 0xFFU)
            {
                context->miso_idle_ff++;
            }
            else
            {
                context->miso_unexpected++;
            }
        }

        context->req_bytes++;
        context->index++;

        if (context->index >= context->length)
        {
            context->req_frames++;
            context->state = SPI_REQ_DONE;
            return SPI_REQ_DONE;
        }

        context->next_action_tick = tick;
        context->state = SPI_REQ_WAIT_GAP;
        return SPI_REQ_WAIT_GAP;
    }

    return context->state;
}

void SpiRequest_Finish(SpiRequestContext *context)
{
    context->data   = (const uint16_t *)0;
    context->length = 0U;
    context->index  = 0U;
    context->state  = SPI_REQ_IDLE;
}

void SpiRequest_GetDiagnostics(const SpiRequestContext *context,
                               SpiRequestDiagnostics *snapshot)
{
    snapshot->req_frames      = context->req_frames;
    snapshot->req_bytes       = context->req_bytes;
    snapshot->miso_idle_ff    = context->miso_idle_ff;
    snapshot->miso_unexpected = context->miso_unexpected;
    snapshot->start_failures  = context->start_failures;
    snapshot->timeouts        = context->timeouts;
    snapshot->last_error      = context->last_error;
}
