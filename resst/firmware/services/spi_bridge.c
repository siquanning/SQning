/* Created by Siquanning */
#include "spi_bridge.h"

void SpiBridge_Init(SpiBridgeContext *context)
{
    UartFrame_Init(&context->uart);
    SpiRequest_Init(&context->spi);
    context->frames_forwarded = 0UL;
    context->spi_timeouts     = 0UL;
}

void SpiBridge_OnRxByte(SpiBridgeContext *context,
                        uint16_t data,
                        uint32_t tick)
{
    UartFrame_OnByte(&context->uart, data, tick);
}

void SpiBridge_OnRxError(SpiBridgeContext *context,
                         uint16_t error_flags,
                         uint32_t tick)
{
    UartFrame_OnError(&context->uart, error_flags, tick);
}

void SpiBridge_Service(SpiBridgeContext *context,
                       uint32_t now,
                       SpiStartFn startFn,
                       SpiCompleteFn completeFn)
{
    UartFrame_Service(&context->uart, now);

    if (UartFrame_IsReady(&context->uart) && SpiRequest_IsIdle(&context->spi))
    {
        const uint16_t *data;
        uint16_t length;
        if (UartFrame_GetReadyData(&context->uart, &data, &length))
        {
            SpiRequest_Start(&context->spi, data, length, now);
        }
    }

    {
        int spiState = SpiRequest_Service(&context->spi, now, startFn, completeFn);

        if (spiState == SPI_REQ_DONE)
        {
            UartFrame_Consume(&context->uart);
            SpiRequest_Finish(&context->spi);
            context->frames_forwarded++;
        }
        else if (spiState == SPI_REQ_TIMEOUT)
        {
            UartFrame_Consume(&context->uart);
            SpiRequest_Finish(&context->spi);
            context->spi_timeouts++;
        }
    }
}

void SpiBridge_GetUartDiagnostics(const SpiBridgeContext *context,
                                  UartFrameDiagnostics *snapshot)
{
    UartFrame_GetDiagnostics(&context->uart, snapshot);
}

void SpiBridge_GetSpiDiagnostics(const SpiBridgeContext *context,
                                 SpiRequestDiagnostics *snapshot)
{
    SpiRequest_GetDiagnostics(&context->spi, snapshot);
}
