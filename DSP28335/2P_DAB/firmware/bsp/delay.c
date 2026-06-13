#include "bsp/delay.h"

void delay_ms(volatile Uint32 ms)
{
    volatile Uint32 i;
    while (ms--) {
        for (i = 0; i < 18750; i++) {
            __asm(" NOP");
        }
    }
}
