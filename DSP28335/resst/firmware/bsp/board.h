#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

void Board_Init(void);

/*
 * Start all ePWM time-base clocks (TBCLKSYNC=1).
 * Counters begin running; 20 kHz EPWM1 ISR fires after this call.
 * PWM outputs remain blocked by TZ until PWM_ReleaseOutput().
 * Must be called after Board_Init() and after EPWM1 PIE/INT is armed.
 */
void PWM_StartTimebase(void);

/*
 * Block all PWM outputs via TZ OST force trip (TZ_FORCE_LO).
 * Does NOT modify AQCSFRC — modulation state is preserved.
 * Safe state for fault, disable, and startup.
 */
void PWM_BlockOutput(void);

/*
 * Release PWM outputs by clearing TZ OST latch.
 * Precondition: modulation shadows must be ready (fast ISR has run ≥ 2 cycles).
 * Does NOT modify AQCSFRC — only removes the TZ safety block.
 */
void PWM_ReleaseOutput(void);

/*
 * Disable PWM outputs immediately (safe state) — convenience wrapper.
 * Equivalent to PWM_BlockOutput(). Kept for compatibility with the
 * state-machine ConsumePwmDisableRequest path.
 */
void PWM_Disable(void);

#endif
