#include "firmware/app/app_context.h"

void AppContext_Init(AppContext *context)
{
    SciRxQueue_Init(&context->sci_rx_queue);
    SpiBridge_Init(&context->spi_bridge);
}
