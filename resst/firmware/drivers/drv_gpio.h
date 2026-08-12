#ifndef DRV_GPIO_H
#define DRV_GPIO_H

#include <stdint.h>

/* LED pins (Port C, active-low) */
void DrvGpio_InitOutput(uint16_t pin, uint16_t initial_high);
void DrvGpio_Set(uint16_t pin);
void DrvGpio_Clear(uint16_t pin);

/*
 * Initialise GPIO27/28/29 as push-pull outputs, all LOW.
 * Call once from Board_Init() after DrvSysCtrl_Init().
 * These pins carry UNI polarity status to the CPLD for H2 bridge gating.
 */
void DrvGpio_InitUniPolarity(void);

/*
 * Write UNI_A_POS / UNI_B_POS / UNI_C_POS in a single EALLOW/EDIS block.
 * Each argument: 1 = m≥0 (A-leg chops, B-leg clamped), 0 = m<0.
 * Must be called from EPWM1 ISR at CTR=ZERO boundary.
 */
void DrvGpio_WriteUniPolarity(uint16_t uni_a, uint16_t uni_b, uint16_t uni_c);

/* ---- CPLD LED heartbeat (Port A, GPIO26) ---- */

/*
 * Initialise GPIO26 as push-pull output LOW.
 * Call once from Board_Init() after DrvSysCtrl_Init().
 */
void DrvGpio_InitCpldLed(void);

/*
 * Toggle GPIO26 level. Caller manages the 1000 ms period;
 * this function only performs the atomic read-invert-write.
 */
void DrvGpio_ToggleCpldLed(void);

/* ---- PWM_ENABLE / FAULT_GATE (Port A, GPIO30) ---- */

/*
 * Initialise GPIO30 as push-pull output LOW (safe: gates blocked).
 * Call once from Board_Init() after DrvSysCtrl_Init().
 */
void DrvGpio_InitFaultGate(void);

/*
 * Set FAULT_GATE level.  Callable from ISR or background.
 *   1 = RUN — CPLD may output gate signals.
 *   0 = FAULT / INIT / STOP — CPLD must force all gates LOW.
 * No EALLOW required — GPASET/GPACLEAR are unprotected registers.
 */
void DrvGpio_WriteFaultGate(uint16_t level);

#endif
