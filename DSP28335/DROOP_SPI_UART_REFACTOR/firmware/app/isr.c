#include "firmware/drivers/drv_timer.h"
#include "firmware/drivers/drv_sci.h"
#include "firmware/drivers/drv_interrupt.h"
#include "firmware/app/isr.h"
#include "firmware/app/diagnostics.h"

static SciRxQueue *g_pSciRxQueue = ((SciRxQueue *)0);

void App_IsrSetQueue(SciRxQueue *queue)
{
    g_pSciRxQueue = queue;
}

__interrupt void App_Timer0Isr(void)
{
    uint32_t t0 = Diagnostics_CycleRead();

    DrvTimer0_OnInterrupt();
    DrvInterrupt_AckGroup1();

    Diagnostics_WcetUpdate(&Diagnostics_Get()->timer0_isr,
                           t0 - Diagnostics_CycleRead());
}

__interrupt void App_SciaRxIsr(void)
{
    uint32_t t0 = Diagnostics_CycleRead();
    uint32_t now = Timebase_Now();
    uint16_t errFlags;
    int      hadError = 0;

    if (g_pSciRxQueue == ((SciRxQueue *)0))
    {
        DrvSci_ClearRxInterrupt();
        DrvInterrupt_AckGroup9();
        Diagnostics_WcetUpdate(&Diagnostics_Get()->sci_rx_isr,
                               t0 - Diagnostics_CycleRead());
        return;
    }

    Diagnostics_Get()->sci_rx_total++;

    if (DrvSci_HasError(&errFlags))
    {
        hadError = 1;
        SciRxQueue_PushFromIsr(g_pSciRxQueue, 0U, errFlags, now);
    }
    else
    {
        uint16_t fifoCount = DrvSci_GetRxFifoCount();
        uint16_t i;

        for (i = 0U; i < fifoCount; i++)
        {
            uint16_t rxStCurr = DrvSci_GetStatus();
            if (DrvSci_StatusHasError(rxStCurr))
            {
                hadError = 1;
                SciRxQueue_PushFromIsr(g_pSciRxQueue, 0U,
                                       DrvSci_GetErrorFromStatus(rxStCurr), now);
                break;
            }
            uint16_t b = DrvSci_ReadByte();
            SciRxQueue_PushFromIsr(g_pSciRxQueue, b, 0U, now);
        }
    }

    if (hadError)
    {
        DrvSci_RecoverRx();
    }

    DrvSci_ClearRxInterrupt();
    DrvInterrupt_AckGroup9();

    Diagnostics_WcetUpdate(&Diagnostics_Get()->sci_rx_isr,
                           t0 - Diagnostics_CycleRead());
}
