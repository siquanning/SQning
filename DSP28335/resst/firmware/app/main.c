#include "firmware/app/app.h"
#include "firmware/bsp/board.h"
#include "firmware/drivers/drv_sci.h"

static AppContext g_app;

void main(void)
{
    App_Init(&g_app);

    /* ---- test: continuous TX on SCI-C, 9600 8N1 ---- */
    for (;;) {
        static const uint16_t msg[] = {
            'H','E','L','L','O',' ','S','C','I','-','C','\r','\n'
        };
        DrvSci_SendBytes(msg, sizeof(msg) / sizeof(msg[0]));

        /* ~500ms busy-wait @ 150 MHz SYSCLKOUT */
        volatile uint32_t d;
        for (d = 0; d < 6000000UL; d++) {}
    }
}
