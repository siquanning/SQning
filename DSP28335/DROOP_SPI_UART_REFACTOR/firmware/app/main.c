#include "firmware/bsp/board.h"
#include "firmware/drivers/drv_timer.h"
#include "firmware/drivers/drv_spi.h"
#include "firmware/services/indicator.h"
#include "firmware/app/isr.h"
#include "firmware/app/app_context.h"
#include "firmware/app/scheduler.h"
#include "firmware/app/diagnostics.h"

static AppContext g_app;
static Scheduler  g_sched;

void main(void)
{
    Board_Init();
    Diagnostics_Init();

    AppContext_Init(&g_app);
    App_IsrSetQueue(&g_app.sci_rx_queue);

    Indicator_Init();

    Scheduler_Init(&g_sched, Timebase_Now());

    {
        uint32_t prev = Diagnostics_CycleRead();

        for (;;)
        {
            uint32_t now = Timebase_Now();
            SciRxItem  item;

            Diagnostics_WcetUpdate(&Diagnostics_Get()->main_loop,
                                   prev - Diagnostics_CycleRead());
            prev = Diagnostics_CycleRead();

            while (SciRxQueue_Pop(&g_app.sci_rx_queue, &item))
            {
                if (item.error_flags != 0U)
                {
                    SpiBridge_OnRxError(&g_app.spi_bridge,
                                        item.error_flags, item.tick);
                }
                else
                {
                    SpiBridge_OnRxByte(&g_app.spi_bridge,
                                       item.data, item.tick);
                }
            }

            SpiBridge_Service(&g_app.spi_bridge, now,
                              DrvSpi_StartByte, DrvSpi_TryCompleteByte);

            Indicator_Service(now);

            if (Scheduler_Take1ms(&g_sched, now))
            {
                /* 1ms: reserved for future fast background tasks */
            }

            if (Scheduler_Take10ms(&g_sched, now))
            {
                /* 10ms: reserved for slow protection, droop outer loop */
            }

            if (Scheduler_Take100ms(&g_sched, now))
            {
                Diagnostics *diag = Diagnostics_Get();
                uint32_t miss1, miss10, miss100;
                Scheduler_GetDiagnostics(&g_sched, &miss1, &miss10, &miss100);
                diag->miss_1ms   = miss1;
                diag->miss_10ms  = miss10;
                diag->miss_100ms = miss100;
                diag->sci_rx_overflow =
                    SciRxQueue_GetOverflowCount(&g_app.sci_rx_queue);
            }
        }
    }
}
