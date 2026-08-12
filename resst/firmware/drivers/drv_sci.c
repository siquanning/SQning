#include "DSP2833x_Device.h"
#include "firmware/bsp/board_pins.h"
#include "firmware/drivers/drv_sci.h"

/* Error mask: PE|OE|FE|BRKDT|RXERROR — excludes RXRDY (bit 6) */
#define DRV_SCI_ERROR_MASK  0x00BCU
#define DRV_SCI_FIFO_OVF     0x8000U

void DrvSci_Init(const DrvSciConfig *config)
{
    uint32_t brr;

    /*
     * SCI-C:
     *   TX = GPIO63 / SCITXDC
     *   RX = GPIO62 / SCIRXDC
     * Baud = 9600, 8N1
     */
    EALLOW;

    /* Enable internal pull-up on both pins */
    GpioCtrlRegs.GPBPUD.bit.GPIO62 = 0U;
    GpioCtrlRegs.GPBPUD.bit.GPIO63 = 0U;

    /* SCI RX asynchronous qualification */
    GpioCtrlRegs.GPBQSEL2.bit.GPIO62 = 3U;

    /* GPIO62 -> SCIRXDC, GPIO63 -> SCITXDC */
    GpioCtrlRegs.GPBMUX2.bit.GPIO62 = 1U;
    GpioCtrlRegs.GPBMUX2.bit.GPIO63 = 1U;

    EDIS;

    /* ---- SCI-C core: 8N1, idle-line mode ---- */
    ScicRegs.SCICCR.all = 0x0007;

    ScicRegs.SCICTL1.all = 0x0000;

    brr = (config->lspclk_hz / (config->baud_rate * 8UL)) - 1UL;
    ScicRegs.SCIHBAUD = (uint16_t)((brr >> 8) & 0xFFU);
    ScicRegs.SCILBAUD = (uint16_t)(brr & 0xFFU);

    /* FIFO: enable enhancements, RX FIFO trigger at 1 byte */
    ScicRegs.SCIFFTX.all  = 0xE060;
    ScicRegs.SCIFFRX.all  = 0x6861;
    ScicRegs.SCIFFCT.all  = 0x0000;

    ScicRegs.SCIFFTX.bit.TXFIFOXRESET = 0;
    ScicRegs.SCIFFTX.bit.TXFIFOXRESET = 1;
    ScicRegs.SCIFFRX.bit.RXFIFORESET  = 0;
    ScicRegs.SCIFFRX.bit.RXFIFORESET  = 1;
    ScicRegs.SCIFFRX.bit.RXFFOVRCLR   = 1;
    ScicRegs.SCIFFRX.bit.RXFFINTCLR   = 1;

    ScicRegs.SCIPRI.all = 0x0000;

    /* Enable receiver, bring SCI out of reset */
    ScicRegs.SCICTL1.bit.RXENA = 1;
    ScicRegs.SCICTL1.all       = 0x0023;
}

uint16_t DrvSci_GetRxFifoCount(void)
{
    uint16_t count = ScicRegs.SCIFFRX.bit.RXFFST;
    if (count > 16U) count = 16U;
    return count;
}

uint16_t DrvSci_GetStatus(void)
{
    return ScicRegs.SCIRXST.all;
}

uint16_t DrvSci_ReadByte(void)
{
    return ScicRegs.SCIRXBUF.all & 0xFFU;
}

bool DrvSci_HasError(uint16_t *error_flags)
{
    uint16_t rxSt  = ScicRegs.SCIRXST.all;
    uint16_t ffRx  = ScicRegs.SCIFFRX.all;
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
    ScicRegs.SCICTL1.bit.SWRESET = 0;
    ScicRegs.SCIFFRX.bit.RXFIFORESET = 0;
    ScicRegs.SCIFFRX.bit.RXFIFORESET = 1;
    ScicRegs.SCIFFRX.bit.RXFFOVRCLR   = 1;
    ScicRegs.SCIFFRX.bit.RXFFINTCLR   = 1;
    ScicRegs.SCICTL1.bit.RXENA = 1;
    ScicRegs.SCICTL1.all       = 0x0023;
}

void DrvSci_ClearRxInterrupt(void)
{
    ScicRegs.SCIFFRX.bit.RXFFINTCLR = 1;
}

void DrvSci_SendByte(uint16_t byte)
{
    while (ScicRegs.SCIFFTX.bit.TXFFST >= 16U) {
        /* wait for TX FIFO space */
    }
    ScicRegs.SCITXBUF = byte & 0x00FFU;
}

void DrvSci_SendBytes(const uint16_t *data, uint16_t len)
{
    uint16_t i;
    for (i = 0U; i < len; ++i) {
        while (ScicRegs.SCIFFTX.bit.TXFFST >= 16U) {
            /* wait */
        }
        ScicRegs.SCITXBUF = data[i] & 0x00FFU;
    }
}
