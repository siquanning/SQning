#ifndef SPI_BRIDGE_H
#define SPI_BRIDGE_H

#include <stdint.h>
#include "uart_frame.h"
#include "spi_request.h"

typedef struct
{
    UartFrameContext  uart;
    SpiRequestContext spi;

    uint32_t frames_forwarded;
    uint32_t spi_timeouts;
} SpiBridgeContext;

void SpiBridge_Init(SpiBridgeContext *context);

void SpiBridge_OnRxByte(SpiBridgeContext *context,
                        uint16_t data,
                        uint32_t tick);

void SpiBridge_OnRxError(SpiBridgeContext *context,
                         uint16_t error_flags,
                         uint32_t tick);

void SpiBridge_Service(SpiBridgeContext *context,
                       uint32_t now,
                       SpiStartFn startFn,
                       SpiCompleteFn completeFn);

void SpiBridge_GetUartDiagnostics(const SpiBridgeContext *context,
                                  UartFrameDiagnostics *snapshot);

void SpiBridge_GetSpiDiagnostics(const SpiBridgeContext *context,
                                 SpiRequestDiagnostics *snapshot);

#endif
