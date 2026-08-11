#ifndef INDICATOR_H
#define INDICATOR_H

#include <stdint.h>

void Indicator_Init(void);
void Indicator_TriggerRx(uint32_t now);
void Indicator_TriggerTx(uint32_t now);
void Indicator_Service(uint32_t now);

#endif
