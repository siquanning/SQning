#ifndef APP_CONTEXT_H
#define APP_CONTEXT_H

#include "firmware/app/sci_rx_queue.h"
#include "firmware/services/spi_bridge.h"

typedef struct
{
    SciRxQueue       sci_rx_queue;
    SpiBridgeContext spi_bridge;
} AppContext;

void AppContext_Init(AppContext *context);

#endif
