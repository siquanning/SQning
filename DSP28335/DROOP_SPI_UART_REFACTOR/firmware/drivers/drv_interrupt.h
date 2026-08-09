#ifndef DRV_INTERRUPT_H
#define DRV_INTERRUPT_H

void DrvInterrupt_Init(void);
void DrvInterrupt_BindTimer0(void (*handler)(void));
void DrvInterrupt_BindSciaRx(void (*handler)(void));
void DrvInterrupt_EnableTimer0(void);
void DrvInterrupt_EnableSciaRx(void);
void DrvInterrupt_AckGroup1(void);
void DrvInterrupt_AckGroup9(void);
void DrvInterrupt_EnableGlobal(void);

#endif
