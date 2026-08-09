#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

void Board_Init(void);
void Board_SciInit(void);
uint32_t Board_TimeNow100us(void);

void Board_SpiInit(void);
int  Board_SpiStartByte(uint16_t txByte);
int  Board_SpiTryCompleteByte(uint16_t *rxByte);

__interrupt void ISRTimer0(void);
__interrupt void ISRSciRx(void);

#endif
