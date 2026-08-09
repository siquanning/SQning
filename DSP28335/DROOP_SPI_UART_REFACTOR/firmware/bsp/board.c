#include "firmware/bsp/board.h"
#include "firmware/bsp/board_config.h"
#include "firmware/bsp/board_pins.h"
#include "firmware/drivers/drv_sysctrl.h"
#include "firmware/drivers/drv_interrupt.h"
#include "firmware/drivers/drv_timer.h"
#include "firmware/drivers/drv_sci.h"
#include "firmware/drivers/drv_spi.h"
#include "firmware/drivers/drv_gpio.h"
#include "firmware/app/isr.h"

void Board_Init(void)
{
    const SysClockConfig clk = { 10U, 2U, 0x0001U, 0x0002U };

#ifdef FLASH
    /*
     * Copy time-critical functions from Flash to RAM while Flash is still at
     * boot-ROM default wait states (conservative, safe at any frequency).
     * The linker defines these symbols from the ramfuncs LOAD/RUN directive.
     */
    {
        extern uint16_t RamfuncsLoadStart;
        extern uint16_t RamfuncsLoadEnd;
        extern uint16_t RamfuncsRunStart;
        extern void MemCopy(uint16_t *src, uint16_t *end, uint16_t *dst);
        MemCopy(&RamfuncsLoadStart, &RamfuncsLoadEnd, &RamfuncsRunStart);
    }
#endif

    if (!DrvSysCtrl_Init(&clk))
    {
        for (;;) { }
    }

#ifdef FLASH
    /*
     * CAUTION: DrvFlash_Init is tagged with CODE_SECTION("ramfuncs").
     * It now executes from RAM (copied above). Configures Flash wait states
     * and pipeline for 150 MHz SYSCLKOUT per TI SPRS439N.
     */
    DrvFlash_Init();
#endif

    DrvInterrupt_Init();

    DrvTimer0_Init(BOARD_SYSCLK_MHZ, BOARD_TIMER0_PERIOD_US);
    DrvInterrupt_BindTimer0(&App_Timer0Isr);
    DrvInterrupt_EnableTimer0();

    {
        const DrvSciConfig sciCfg = { BOARD_LSPCLK_HZ, BOARD_SCIA_BAUD };
        DrvSci_Init(&sciCfg);
    }
    DrvInterrupt_BindSciaRx(&App_SciaRxIsr);
    DrvInterrupt_EnableSciaRx();

    {
        const DrvSpiConfig spiCfg = { BOARD_SPIA_BRR };
        DrvSpi_Init(&spiCfg);
    }

    DrvInterrupt_EnableGlobal();
    DrvTimer0_Start();
}
