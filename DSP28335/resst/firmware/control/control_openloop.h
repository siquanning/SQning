#ifndef CONTROL_OPENLOOP_H
#define CONTROL_OPENLOOP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OPENLOOP_LUT_SIZE   1200U
#define OPENLOOP_LUT_STEP   3U
#define OPEN_LOOP_M_PERMILL_A 200
#define OPEN_LOOP_M_PERMILL_B 200
#define OPEN_LOOP_M_PERMILL_C 200

/*
 * Generate 1200-point Q15 sine LUT at init time.
 * Uses FPU — called once from App_Init, never in ISR.
 */
void OpenLoop_InitSine(void);

/*
 * Advance phase index by 3 and fill mabc[] with
 * 3-phase 50 Hz sine references scaled by OPEN_LOOP_M_PERMILL.
 *
 *   20000 Hz / 400 steps = 50 Hz fundamental
 *
 * Called from App_Epwm1Isr at 20 kHz.
 * ~15 integer ops, no float, no division in ISR path.
 */
void OpenLoop_GenerateSine(int16_t mabc[3]);

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_OPENLOOP_H */
