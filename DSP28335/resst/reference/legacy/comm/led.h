#ifndef LED_H
#define LED_H

#include <stdint.h>

void Led_Init(void);
void Led_TriggerRx(uint32_t nowTick);
void Led_TriggerTx(uint32_t nowTick);
void Led_Service(uint32_t nowTick);

#endif
