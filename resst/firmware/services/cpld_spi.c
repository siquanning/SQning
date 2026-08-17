/* Created by Siquanning */
#include "firmware/services/cpld_spi.h"
#include "firmware/drivers/drv_spi.h"

/*
 * Blocking single-byte SPI send.
 * Busy-waits on BUFFULL_FLAG and INT_FLAG — foreground use only.
 */
static void CPLD_SendByte(uint16_t byte_val)
{
    (void)DrvSpi_TransferByteBlocking(byte_val);
}

void CPLD_Init(void)
{
    /* SPI-A already initialized by DrvSpi_Init in Board_Init. */
}

void CPLD_WriteReg16(uint16_t addr, int16_t value)
{
    uint16_t raw = (uint16_t)value;

    CPLD_SendByte(0x02U);                   /* CMD: WRITE */
    CPLD_SendByte(addr & 0xFFU);            /* ADDR */
    CPLD_SendByte((raw >> 8) & 0xFFU);      /* DATA_H (big-endian) */
    CPLD_SendByte(raw & 0xFFU);             /* DATA_L */
}

void CPLD_Commit(void)
{
    /* bit0=H2使能，bit1=H1透传使能 */
    CPLD_WriteReg16(3U, 0x0003);
}
