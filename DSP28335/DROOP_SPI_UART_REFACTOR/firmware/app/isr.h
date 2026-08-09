#ifndef APP_ISR_H
#define APP_ISR_H

#include "firmware/app/sci_rx_queue.h"

__interrupt void App_Timer0Isr(void);
__interrupt void App_SciaRxIsr(void);

void App_IsrSetQueue(SciRxQueue *queue);

#endif
