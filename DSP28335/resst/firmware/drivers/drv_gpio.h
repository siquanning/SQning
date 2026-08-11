#ifndef DRV_GPIO_H
#define DRV_GPIO_H

#include <stdint.h>

void DrvGpio_InitOutput(uint16_t pin, uint16_t initial_high);
void DrvGpio_Set(uint16_t pin);
void DrvGpio_Clear(uint16_t pin);

#endif
