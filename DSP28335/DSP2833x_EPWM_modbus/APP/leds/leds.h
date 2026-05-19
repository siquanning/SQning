/*
 * leds.h — LED control for GPIO64-68 (active-low on DST28335 board)
 *
 * GPIO10/11 are reserved for ePWM6A/B and must NOT be used as LEDs.
 */

#ifndef _LEDS_H_
#define _LEDS_H_

#include "DSP2833x_Device.h"
#include "DSP2833x_Examples.h"

void LED_Init(void);

// ---- 内联控制函数（替代宏，支持类型检查和调试）----------------------------

static inline void LED1_Off(void)    { GpioDataRegs.GPCSET.bit.GPIO68    = 1; }
static inline void LED1_On(void)     { GpioDataRegs.GPCCLEAR.bit.GPIO68  = 1; }
static inline void LED1_Toggle(void) { GpioDataRegs.GPCTOGGLE.bit.GPIO68 = 1; }

static inline void LED2_Off(void)    { GpioDataRegs.GPCSET.bit.GPIO67    = 1; }
static inline void LED2_On(void)     { GpioDataRegs.GPCCLEAR.bit.GPIO67  = 1; }
static inline void LED2_Toggle(void) { GpioDataRegs.GPCTOGGLE.bit.GPIO67 = 1; }

static inline void LED3_Off(void)    { GpioDataRegs.GPCSET.bit.GPIO66    = 1; }
static inline void LED3_On(void)     { GpioDataRegs.GPCCLEAR.bit.GPIO66  = 1; }
static inline void LED3_Toggle(void) { GpioDataRegs.GPCTOGGLE.bit.GPIO66 = 1; }

static inline void LED4_Off(void)    { GpioDataRegs.GPCSET.bit.GPIO65    = 1; }
static inline void LED4_On(void)     { GpioDataRegs.GPCCLEAR.bit.GPIO65  = 1; }
static inline void LED4_Toggle(void) { GpioDataRegs.GPCTOGGLE.bit.GPIO65 = 1; }

static inline void LED5_Off(void)    { GpioDataRegs.GPCSET.bit.GPIO64    = 1; }
static inline void LED5_On(void)     { GpioDataRegs.GPCCLEAR.bit.GPIO64  = 1; }
static inline void LED5_Toggle(void) { GpioDataRegs.GPCTOGGLE.bit.GPIO64 = 1; }

#endif /* _LEDS_H_ */
