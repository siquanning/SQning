#ifndef DRV_TIMER_H
#define DRV_TIMER_H

#include <stdint.h>

void DrvTimer0_Init(uint32_t sysclk_mhz, uint32_t period_us);
void DrvTimer0_Start(void);
void DrvTimer0_OnInterrupt(void);
uint32_t Timebase_Now(void);

#endif
