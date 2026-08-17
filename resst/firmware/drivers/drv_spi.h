#ifndef DRV_SPI_H
#define DRV_SPI_H

#include <stdint.h>

typedef struct
{
    uint16_t brr;
} DrvSpiConfig;

void DrvSpi_Init(const DrvSpiConfig *config);
int  DrvSpi_StartByte(uint16_t tx_byte);
int  DrvSpi_TryCompleteByte(uint16_t *rx_byte);
uint16_t DrvSpi_TransferByteBlocking(uint16_t tx_byte);

#endif
