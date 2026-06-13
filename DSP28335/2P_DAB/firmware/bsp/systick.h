#ifndef BSP_SYSTICK_H
#define BSP_SYSTICK_H

// 1kHz systick via CPU Timer 0, 1ms period, for control loop scheduling.

typedef void (*systick_callback_t)(void);

void systick_init(void);
void systick_register_callback(systick_callback_t cb);

// ISR — registered in PIE vector table, calls the user callback
__interrupt void systick_isr(void);

#endif
