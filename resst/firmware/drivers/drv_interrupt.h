#ifndef DRV_INTERRUPT_H
#define DRV_INTERRUPT_H

#include <stdint.h>

void DrvInterrupt_Init(void);
void DrvInterrupt_BindTimer0(void (*handler)(void));
void DrvInterrupt_BindScicRx(void (*handler)(void));
void DrvInterrupt_BindAdcSeq1(void (*handler)(void));
void DrvInterrupt_EnableTimer0(void);
void DrvInterrupt_EnableScicRx(void);
void DrvInterrupt_EnableAdcSeq1(void);
void DrvInterrupt_BindEpwm1(void (*handler)(void));
void DrvInterrupt_EnableEpwm1(void);
void DrvInterrupt_BindEpwmTz(uint32_t module, void (*handler)(void));
void DrvInterrupt_EnableEpwmTz(uint32_t module);
void DrvInterrupt_AckGroup1(void);
void DrvInterrupt_AckGroup2(void);
void DrvInterrupt_AckGroup3(void);
void DrvInterrupt_AckGroup8(void);
void DrvInterrupt_EnableGlobal(void);

#endif
