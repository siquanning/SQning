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

/* 仅释放选中测试相的两路ePWM，其余四路继续保持独立OST封锁。 */
uint16_t PWM_ReleaseSelectedPhase(uint16_t phase);

/* 清除ePWM1~6的OST并重新武装保护，最后才拉高GPIO30。 */
uint16_t PWM_ReleaseThreePhase(void);

/*
 * Check whether hardware trip inputs TZ1 (GPIO12) and TZ2 (GPIO13)
 * are in the safe (non-fault) state.
 *
 * Reads the GPIO input buffer directly — does NOT use TZFLG latched status.
 * Returns 1 only when both inputs are at the inactive level as defined by
 * BOARD_TZ1_ACTIVE_LEVEL / BOARD_TZ2_ACTIVE_LEVEL in board_config.h.
 * When BOARD_PWM_ADC_HW_CONFIRMED == 0, always returns 1.
 */
uint16_t PWM_AreTripInputsClear(void);

#endif
