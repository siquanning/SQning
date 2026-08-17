#ifndef DRV_GPIO_H
#define DRV_GPIO_H

#include <stdint.h>

/* LED pins (Port C, active-low) */
void DrvGpio_InitOutput(uint16_t pin, uint16_t initial_high);
void DrvGpio_Set(uint16_t pin);
void DrvGpio_Clear(uint16_t pin);

/* Toggle LED pin (GPIO67/68) level. Atomic read-invert-write on Port C. */
void DrvGpio_Toggle(uint16_t pin);

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

/* ---- Run/Stop button + run state indicator (Port A, GPIO21/20) ---- */

/*
 * Initialise GPIO21 as digital input (EQEP1B mux off).
 * Internal pull-up enabled — released reads HIGH, pressed-to-GND reads LOW.
 * Call once from Board_Init() after DrvSysCtrl_Init().
 */
void DrvGpio_InitRunButton(void);

/*
 * Read GPIO21 raw input level: 1 = pin high, 0 = pin low.
 * Caller compares against BOARD_RUN_BTN_ACTIVE_LEVEL.
 */
uint16_t DrvGpio_ReadRunButton(void);

/*
 * Initialise GPIO20 as push-pull output LOW (active-high LED off).
 * The API accepts logical run state and applies the LED polarity internally.
 * Call once from Board_Init() after DrvSysCtrl_Init().
 */
void DrvGpio_InitRunState(void);

/*
 * Write GPIO20 run indication (GPASET/GPACLEAR, no EALLOW required).
 *   level = 0 → GPIO LOW,  LED OFF
 *   level = 1 → GPIO HIGH, LED ON
 */
void DrvGpio_WriteRunState(uint16_t level);

/* ---- Grid input switch + precharge bypass (Port B, GPIO42/44) ---- */

/* GPIO42：三相输入总开关，同时控制S1/S2/S3；初始化为LOW，三个开关均断开。 */
void DrvGpio_InitGridSwitch(void);

/* GPIO42写接口：0=S1/S2/S3全断，1=S1/S2/S3全合。 */
void DrvGpio_WriteGridSwitch(uint16_t on);

/* GPIO44：预充电阻旁路总开关，同时控制S4/S5/S6；初始化为LOW，电阻未旁路。 */
void DrvGpio_InitPrechargeBypass(void);

/* GPIO44写接口：0=旁路开关全断，预充电阻串入；1=旁路开关全合。 */
void DrvGpio_WritePrechargeBypass(uint16_t on);

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
