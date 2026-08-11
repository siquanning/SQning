#ifndef DRV_TIMER_H
#define DRV_TIMER_H

#include <stdint.h>

/* ---- Timer0: 100us system tick ---- */
void DrvTimer0_Init(uint32_t sysclk_mhz, uint32_t period_us);
void DrvTimer0_Start(void);
void DrvTimer0_OnInterrupt(void);
uint32_t Timebase_Now(void);

/* ---- Timer2: free-running cycle counter (WCET measurement) ---- */
void DrvTimer2_CycleInit(void);
uint32_t DrvTimer2_CycleRead(void);

#endif
