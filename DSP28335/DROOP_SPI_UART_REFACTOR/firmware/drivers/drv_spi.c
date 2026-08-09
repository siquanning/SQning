#include "DSP2833x_Device.h"
#include "firmware/bsp/board_pins.h"
#include "firmware/drivers/drv_spi.h"

void DrvSpi_Init(const DrvSpiConfig *config)
{
    /* ---- GPIO16 (MOSI), GPIO17 (MISO), GPIO18 (SCK) ---- */
    EALLOW;

    GpioCtrlRegs.GPAMUX2.bit.GPIO16 = 1;
    GpioCtrlRegs.GPAMUX2.bit.GPIO17 = 1;
    GpioCtrlRegs.GPAMUX2.bit.GPIO18 = 1;

    GpioCtrlRegs.GPAQSEL2.bit.GPIO17 = 3;

    GpioCtrlRegs.GPADIR.bit.GPIO16 = 1;
    GpioCtrlRegs.GPADIR.bit.GPIO17 = 0;
    GpioCtrlRegs.GPADIR.bit.GPIO18 = 1;

    GpioCtrlRegs.GPAPUD.bit.GPIO16 = 0;
    GpioCtrlRegs.GPAPUD.bit.GPIO17 = 0;
    GpioCtrlRegs.GPAPUD.bit.GPIO18 = 0;

    /* GPIO19 is NOT configured — no CS, no MUX change */
    EDIS;

    /* ---- SPI-A core: Master, Mode 0, 8-bit, MSB first, no FIFO ---- */
    SpiaRegs.SPICCR.bit.SPISWRESET  = 0;
    SpiaRegs.SPICCR.bit.CLKPOLARITY = 0;
    SpiaRegs.SPICCR.bit.SPICHAR     = 7;
    SpiaRegs.SPICCR.bit.SPILBK      = 0;

    SpiaRegs.SPICTL.bit.CLK_PHASE      = 0;
    SpiaRegs.SPICTL.bit.MASTER_SLAVE   = 1;
    SpiaRegs.SPICTL.bit.TALK           = 1;
    SpiaRegs.SPICTL.bit.SPIINTENA      = 0;
    SpiaRegs.SPICTL.bit.OVERRUNINTENA  = 0;

    SpiaRegs.SPIBRR = config->brr;

    /* SPIRST=1 (release TX/RX channels), SPIFFENA=0 (no FIFO).
     * REGRESSION GUARD: SPIRST (bit 15) MUST be 1. */
    SpiaRegs.SPIFFTX.all = 0x8000U;
    SpiaRegs.SPIFFRX.all = 0x0000U;
    SpiaRegs.SPIFFCT.all = 0x0000U;

    SpiaRegs.SPIPRI.bit.FREE = 1;

    /* Release SPI from reset */
    SpiaRegs.SPICCR.bit.SPISWRESET = 1;
}

int DrvSpi_StartByte(uint16_t tx_byte)
{
    if (SpiaRegs.SPISTS.bit.BUFFULL_FLAG == 1)
    {
        return 0;
    }
    SpiaRegs.SPITXBUF = tx_byte << 8;
    return 1;
}

int DrvSpi_TryCompleteByte(uint16_t *rx_byte)
{
    if (SpiaRegs.SPISTS.bit.INT_FLAG == 0)
    {
        return 0;
    }
    *rx_byte = SpiaRegs.SPIRXBUF & 0x00FFU;
    return 1;
}
