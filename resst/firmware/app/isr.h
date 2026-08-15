#ifndef APP_ISR_H
#define APP_ISR_H

#include <stdint.h>
#include "firmware/app/sci_rx_queue.h"
#include "firmware/control/control_common.h"
#include "firmware/app/state_machine.h"
#include "firmware/app/param_manager.h"
#include "firmware/app/telemetry.h"

/* Vdc raw ADC globals — ISR writes directly, debugger watches these */
extern volatile uint16_t g_vdc_raw[6];
extern volatile uint16_t g_vac_raw[3];
extern volatile uint16_t g_iac_raw[3];
extern volatile uint32_t g_adc_frame_count;

__interrupt void App_Timer0Isr(void);
__interrupt void App_ScicRxIsr(void);
__interrupt void App_AdcIsr(void);
__interrupt void App_Epwm1Isr(void);
__interrupt void App_EpwmTzIsr(void);

void App_IsrSetQueue(SciRxQueue *queue);
void App_IsrSetControl(ControlContext *control);
void App_IsrSetStateMachine(StateMachine *sm);
void App_IsrSetParamManager(ParamManager *pm);
void App_IsrSetTelemetry(Telemetry *t);

#endif
