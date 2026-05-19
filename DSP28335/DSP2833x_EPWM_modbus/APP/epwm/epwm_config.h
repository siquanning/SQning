/*
 * epwm_config.h — ePWM6 configuration for PWM output
 */

#ifndef APP_EPWM_EPWM_CONFIG_H_
#define APP_EPWM_EPWM_CONFIG_H_

#include "DSP2833x_Device.h"
#include "DSP2833x_Examples.h"

// ---- ePWM6 时基参数 ----------------------------
// TBCLK = SYSCLK / (HSPCLKDIV * CLKDIV) = 150MHz / (4 * 1) = 37.5MHz
#define EPWM6_TBPRD        18749     // 1kHz  up-down: TBCLK/(2*f) - 1
#define EPWM6_CMPA_DEFAULT 9375      // 50%   duty at 1kHz

// ---- 输出 GPIO --------------------------------
#define EPWM6A_GPIO         10

void Init_EPWM6_1kHz_50Percent(void);

#endif /* APP_EPWM_EPWM_CONFIG_H_ */
