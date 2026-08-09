#include "DSP2833x_Device.h"
#include "firmware/bsp/board_pins.h"
#include "firmware/drivers/drv_sci.h"

/* Error mask: PE|OE|FE|BRKDT|RXERROR — excludes RXRDY (bit 6) */
#define DRV_SCI_ERROR_MASK  0x00BCU
#define DRV_SCI_FIFO_OVF     0x8000U

void DrvSci_Init(const DrvSciConfig *config)
{
    uint32_t brr;

    /* ---- GPIO35 (SCITXDA) / GPIO36 (SCIRXDA) ---- */
    EALLOW;

    GpioCtrlRegs.GPBMUX1.bit.GPIO35 = 1;
    GpioCtrlRegs.GPBMUX1.bit.GPIO36 = 1;

    GpioCtrlRegs.GPBQSEL1.bit.GPIO35 = 3;
    GpioCtrlRegs.GPBQSEL1.bit.GPIO36 = 3;

    GpioCtrlRegs.GPBDIR.bit.GPIO35 = 1;
    GpioCtrlRegs.GPBDIR.bit.GPIO36 = 0;

    GpioCtrlRegs.GPBPUD.bit.GPIO35 = 0;
    GpioCtrlRegs.GPBPUD.bit.GPIO36 = 0;

    EDIS;

    /* ---- SCI-A core: 8N1, idle-line mode ---- */
    SciaRegs.SCICCR.all = 0x0007;

    SciaRegs.SCICTL1.all = 0x0000;

    brr = (config->lspclk_hz / (config->baud_rate * 8UL)) - 1UL;
    SciaRegs.SCIHBAUD = (uint16_t)((brr >> 8) & 0xFFU);
    SciaRegs.SCILBAUD = (uint16_t)(brr & 0xFFU);

    /* FIFO: enable enhancements, RX FIFO trigger at 1 byte */
    SciaRegs.SCIFFTX.all  = 0xE060;
    SciaRegs.SCIFFRX.all  = 0x6061;
    SciaRegs.SCIFFCT.all  = 0x0000;

    SciaRegs.SCIFFTX.bit.TXFIFOXRESET = 0;
    SciaRegs.SCIFFTX.bit.TXFIFOXRESET = 1;
    SciaRegs.SCIFFRX.bit.RXFIFORESET  = 0;
    SciaRegs.SCIFFRX.bit.RXFIFORESET  = 1;
    SciaRegs.SCIFFRX.bit.RXFFOVRCLR   = 1;
    SciaRegs.SCIFFRX.bit.RXFFINTCLR   = 1;

    SciaRegs.SCIPRI.all = 0x0000;

    /* Enable receiver, bring SCI out of reset */
    SciaRegs.SCICTL1.bit.RXENA = 1;
    SciaRegs.SCICTL1.all       = 0x0023;
}

uint16_t DrvSci_GetRxFifoCount(void)
{
    uint16_t count = SciaRegs.SCIFFRX.bit.RXFFST;
    if (count > 16U) count = 16U;
    return count;
}

uint16_t DrvSci_GetStatus(void)
{
    return SciaRegs.SCIRXST.all;
}

uint16_t DrvSci_ReadByte(void)
{
    return SciaRegs.SCIRXBUF.all & 0xFFU;
}

bool DrvSci_HasError(uint16_t *error_flags)
{
    uint16_t rxSt  = SciaRegs.SCIRXST.all;
    uint16_t ffRx  = SciaRegs.SCIFFRX.all;
    uint16_t err   = rxSt & DRV_SCI_ERROR_MASK;
    uint16_t fifoOvf = ffRx & DRV_SCI_FIFO_OVF;

    if (err == 0U && fifoOvf == 0U)
    {
        return false;
    }

    if (error_flags != ((uint16_t *)0))
    {
        *error_flags = err | (fifoOvf ? 0x8000U : 0U);
    }
    return true;
}

bool DrvSci_StatusHasError(uint16_t status)
{
    return ((status & DRV_SCI_ERROR_MASK) != 0U);
}

uint16_t DrvSci_GetErrorFromStatus(uint16_t status)
{
    return status & DRV_SCI_ERROR_MASK;
}

void DrvSci_RecoverRx(void)
{
    SciaRegs.SCICTL1.bit.SWRESET = 0;
    SciaRegs.SCIFFRX.bit.RXFIFORESET = 0;
    SciaRegs.SCIFFRX.bit.RXFIFORESET = 1;
    SciaRegs.SCIFFRX.bit.RXFFOVRCLR   = 1;
    SciaRegs.SCIFFRX.bit.RXFFINTCLR   = 1;
    SciaRegs.SCICTL1.bit.RXENA = 1;
    SciaRegs.SCICTL1.all       = 0x0023;
}

void DrvSci_ClearRxInterrupt(void)
{
    SciaRegs.SCIFFRX.bit.RXFFINTCLR = 1;
}
