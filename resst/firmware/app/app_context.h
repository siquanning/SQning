/* Created by Siquanning */
#ifndef APP_CONTEXT_H
#define APP_CONTEXT_H

#include "firmware/app/sci_rx_queue.h"
#include "firmware/services/spi_bridge.h"
#include "firmware/control/control_common.h"
#include "firmware/app/state_machine.h"
#include "firmware/app/param_manager.h"
#include "firmware/app/telemetry.h"
#include "firmware/services/pll_host_protocol.h"

typedef struct
{
    SciRxQueue       sci_rx_queue;
    SpiBridgeContext spi_bridge;
    ControlContext   control;
    StateMachine     state_machine;
    ParamManager     param_manager;
    Telemetry        telemetry;
    PllHostProtocol  pll_host_protocol;
} AppContext;

void AppContext_Init(AppContext *context);

#endif
