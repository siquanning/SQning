/*
 * app_modbus.h — Application-layer Modbus register mapping
 *
 * Maps ePWM6 parameters (frequency, duty cycle) to Modbus registers:
 *   40001 (Holding[0]): PWM frequency (Hz)
 *   40002 (Holding[1]): PWM duty cycle (0.1% units)
 *   30001 (Input[0]):   Actual PWM frequency (Hz) — read-only
 *   30002 (Input[1]):   Actual PWM duty cycle (0.1% units) — read-only
 */

#ifndef APP_MODBUS_APP_MODBUS_H_
#define APP_MODBUS_APP_MODBUS_H_

#include <stdbool.h>
#include <stdint.h>
#include "DSP2833x_Device.h"
#include "DSP2833x_EPwm.h"
#include "modbus_slave.h"

// ---- ePWM6 时基 ---------------------------------
#define TBCLK_FREQ           37500000UL   // HSPCLK/4
#define PWM_FREQ_MIN                10U    // Hz
#define PWM_FREQ_MAX             50000U    // Hz
#define PWM_TBPRD_MIN               10U

// ---- 无效值哨兵 ---------------------------------
#define TBPRD_INVALID           0xFFFFU

void MB_InitRegs(void);
bool MB_ApplyRegChanges(void);
void MB_ReadInputRegs(void);

#endif /* APP_MODBUS_APP_MODBUS_H_ */
