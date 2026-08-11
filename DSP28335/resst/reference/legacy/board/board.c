#include "board.h"
#include "firmware/services/uart_frame.h"
#include "config/comm_config.h"
#include "DSP2833x_Device.h"

static volatile uint32_t g_tick100us;

void Board_Init(void)
{
    g_tick100us = 0UL;

    ConfigCpuTimer(&CpuTimer0, 150.0F, 100.0F);

    EALLOW;
    PieVectTable.TINT0 = &ISRTimer0;
    EDIS;

    PieCtrlRegs.PIEIER1.bit.INTx7 = 1;
    IER |= M_INT1;

    CpuTimer0Regs.TCR.bit.TSS = 0;
}

void Board_SciInit(void)
{
    /* ---- GPIO35 (SCITXDA) and GPIO36 (SCIRXDA) ---- */
    EALLOW;

    /* MUX = 1 for SCI-A function on GPIO35/36 */
    GpioCtrlRegs.GPBMUX1.bit.GPIO35 = 1;
    GpioCtrlRegs.GPBMUX1.bit.GPIO36 = 1;

    /* QSEL = 3 (asynchronous input qualification) */
    GpioCtrlRegs.GPBQSEL1.bit.GPIO35 = 3;
    GpioCtrlRegs.GPBQSEL1.bit.GPIO36 = 3;

    /* TX pin = output; RX pin = input */
    GpioCtrlRegs.GPBDIR.bit.GPIO35 = 1;
    GpioCtrlRegs.GPBDIR.bit.GPIO36 = 0;

    /* Pull-up enabled: clear the pull-up disable bit */
    GpioCtrlRegs.GPBPUD.bit.GPIO35 = 0;
    GpioCtrlRegs.GPBPUD.bit.GPIO36 = 0;

    EDIS;

    /* ---- SCI-A core: 9600 bps, 8N1, idle-line mode ---- */
    SciaRegs.SCICCR.all = 0x0007;
    /* SCICHAR=7 (8-bit), ADDRIDLE_MODE=0 (idle-line), 0 parity, 1 stop */

    SciaRegs.SCICTL1.all = 0x0000;

    SciaRegs.SCIHBAUD = 0x01;
    SciaRegs.SCILBAUD = 0xE7;

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
    SciaRegs.SCICTL1.all        = 0x0023;
    /* SCICTL1 bitfields (F28335 TRM SPRUFZ5A):
     *   bit0 RXENA=1   bit1 TXENA=1     bit5 SWRESET=1
     *   bit6 RXERRINTENA=0  — error status is polled inside SCI RX ISR */

    /* ---- PIE / interrupt registration ---- */
    EALLOW;
    PieVectTable.SCIRXINTA = &ISRSciRx;
    EDIS;

    PieCtrlRegs.PIEIER9.bit.INTx1 = 1;   /* PIE 9.1 */
    IER |= M_INT9;                         /* CPU INT9 */
}

__interrupt void ISRSciRx(void)
{
    uint16_t rxSt   = SciaRegs.SCIRXST.all;
    uint16_t ffRx   = SciaRegs.SCIFFRX.all;
    uint32_t now    = Board_TimeNow100us();

    /*
     * SCIRXST error bits (F28335 SCI Ref Guide, SPRUFZ5A §4.4):
     *   bit2  PE       — parity error
     *   bit3  OE       — overrun error
     *   bit4  FE       — framing error
     *   bit5  BRKDT    — break detect
     *   bit7  RXERROR  — OR of PE|OE|FE|BRKDT
     * SCIFFRX error bits:
     *   bit15 RXFFOVF  — RX FIFO overflow
     *
     * bit6 RXRDY is a normal status flag and MUST NOT be treated as an error.
     */
    uint16_t errMask = rxSt & 0x00BCU;  /* PE|OE|FE|BRKDT|RXERROR — excludes RXRDY */
    uint16_t fifoOvf = ffRx & 0x8000U;  /* RXFFOVF */
    int      hadError = 0;

    if (errMask != 0U || fifoOvf != 0U)
    {
        hadError = 1;
        uint16_t errF = errMask | (fifoOvf ? 0x8000U : 0U);
        UartFrame_OnError(errF, now);
    }
    else
    {
        /* Bounded FIFO read: snapshot depth, read at most that many */
        uint16_t fifoCount = SciaRegs.SCIFFRX.bit.RXFFST;
        if (fifoCount > 16U) fifoCount = 16U;

        uint16_t i;
        for (i = 0U; i < fifoCount; i++)
        {
            /* Re-check SCIRXST per byte: FIFO-top advances with each read */
            uint16_t rxStCurr = SciaRegs.SCIRXST.all;
            if ((rxStCurr & 0x00BCU) != 0U)
            {
                hadError = 1;
                UartFrame_OnError(rxStCurr & 0x00BCU, now);
                break;
            }
            uint16_t b = SciaRegs.SCIRXBUF.all & 0xFFU;
            UartFrame_OnByte(b, now);
        }
    }

    if (hadError)
    {
        /* Full SCI RX recovery: SWRESET=0 → fifo reset → clear → SWRESET=1 */
        SciaRegs.SCICTL1.bit.SWRESET = 0;
        SciaRegs.SCIFFRX.bit.RXFIFORESET = 0;
        SciaRegs.SCIFFRX.bit.RXFIFORESET = 1;
        SciaRegs.SCIFFRX.bit.RXFFOVRCLR   = 1;
        SciaRegs.SCIFFRX.bit.RXFFINTCLR   = 1;
        SciaRegs.SCICTL1.bit.RXENA = 1;
        SciaRegs.SCICTL1.all       = 0x0023;
    }

    SciaRegs.SCIFFRX.bit.RXFFINTCLR = 1;
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP9;
}

__interrupt void ISRTimer0(void)
{
    g_tick100us++;

    CpuTimer0Regs.TCR.bit.TIF = 1;
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
}

uint32_t Board_TimeNow100us(void)
{
    uint32_t first, second;
    do
    {
        first = g_tick100us;
        second = g_tick100us;
    } while (first != second);
    return first;
}

void Board_SpiInit(void)
{
    /* ---- GPIO16 (SPISIMOA/MOSI), GPIO17 (SPISOMIA/MISO), GPIO18 (SPICLKA/SCK) ---- */
    EALLOW;

    /* MUX = 1 for SPI-A on GPIO16/17/18 (Port A, MUX2 covers GPIO16-31) */
    GpioCtrlRegs.GPAMUX2.bit.GPIO16 = 1;
    GpioCtrlRegs.GPAMUX2.bit.GPIO17 = 1;
    GpioCtrlRegs.GPAMUX2.bit.GPIO18 = 1;

    /* Asynchronous input qualification on MISO (Port A, QSEL2 covers GPIO16-31) */
    GpioCtrlRegs.GPAQSEL2.bit.GPIO17 = 3;

    /* Direction: MOSI out, MISO in, SCK out (Port A, GPADIR covers GPIO0-31) */
    GpioCtrlRegs.GPADIR.bit.GPIO16 = 1;
    GpioCtrlRegs.GPADIR.bit.GPIO17 = 0;
    GpioCtrlRegs.GPADIR.bit.GPIO18 = 1;

    /* Pull-up enabled on all three (Port A, GPAPUD covers GPIO0-31) */
    GpioCtrlRegs.GPAPUD.bit.GPIO16 = 0;
    GpioCtrlRegs.GPAPUD.bit.GPIO17 = 0;
    GpioCtrlRegs.GPAPUD.bit.GPIO18 = 0;

    /* GPIO19 is NOT configured — no CS, no MUX change, no direction override */
    EDIS;

    /* ---- SPI-A core: Master, Mode 0, 8-bit, MSB first, no FIFO, no interrupts ---- */
    SpiaRegs.SPICCR.bit.SPISWRESET = 0;
    SpiaRegs.SPICCR.bit.CLKPOLARITY = 0;
    SpiaRegs.SPICCR.bit.SPICHAR     = 7;
    SpiaRegs.SPICCR.bit.SPILBK      = 0;

    SpiaRegs.SPICTL.bit.CLK_PHASE      = 0;
    SpiaRegs.SPICTL.bit.MASTER_SLAVE   = 1;
    SpiaRegs.SPICTL.bit.TALK           = 1;
    SpiaRegs.SPICTL.bit.SPIINTENA      = 0;
    SpiaRegs.SPICTL.bit.OVERRUNINTENA  = 0;

    SpiaRegs.SPIBRR = SPI_BRR;

    /* SPIFFTX: SPIRST=1 (release TX/RX channels), SPIFFENA=0 (no FIFO).
     * REGRESSION GUARD: SPIFFTX.bit.SPIRST (bit 15) MUST be 1.  Writing 0x0000
     * here holds the SPI transmit and receive channels in reset — no SCK, no
     * INT_FLAG, and every transfer times out.  Verify with:
     *   assert((SpiaRegs.SPIFFTX.all & 0x8000U) == 0x8000U);
     */
    SpiaRegs.SPIFFTX.all = 0x8000U;
    SpiaRegs.SPIFFRX.all = 0x0000U;
    SpiaRegs.SPIFFCT.all = 0x0000U;

    SpiaRegs.SPIPRI.bit.FREE = 1;

    /* Release SPI from reset */
    SpiaRegs.SPICCR.bit.SPISWRESET = 1;
}

int Board_SpiStartByte(uint16_t txByte)
{
    if (SpiaRegs.SPISTS.bit.BUFFULL_FLAG == 1)
    {
        return 0;
    }
    SpiaRegs.SPITXBUF = txByte << 8;
    return 1;
}

int Board_SpiTryCompleteByte(uint16_t *rxByte)
{
    if (SpiaRegs.SPISTS.bit.INT_FLAG == 0)
    {
        return 0;
    }
    *rxByte = SpiaRegs.SPIRXBUF & 0x00FFU;
    return 1;
}
