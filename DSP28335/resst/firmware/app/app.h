#ifndef APP_H
#define APP_H

#include "firmware/app/app_context.h"
#include "firmware/app/scheduler.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * App_Init — one-time platform initialization.
 *
 * Sequence:
 *   Board_Init → Diagnostics_Init → AppContext_Init → ISR wiring
 *   → Indicator_Init → Scheduler_Init → StateMachine BOOT→INIT→STANDBY
 *   → StateMachine_RequestRun
 *
 * On return the system is in RUN (or logical RUN when HW_CONFIRMED=0)
 * and the main loop can be entered.
 */
void App_Init(AppContext *app);

/*
 * App_RunForever — infinite main loop.
 *
 * Background rate groups:
 *   Foreground: drain SCI RX queue, service SpiBridge, service Indicator
 *   1ms:        parameter commit service, PWM disable consumption
 *   10ms:       state machine service
 *   100ms:      diagnostics snapshot, telemetry, fault detection
 *
 * Never returns.
 */
void App_RunForever(AppContext *app);

/*
 * Per-iteration foreground: SCI queue → SpiBridge → Indicator.
 * Public for host-test visibility; normally called only from App_RunForever.
 */
void App_ServiceForeground(AppContext *app, uint32_t now);

/*
 * 1ms background: parameter commit + PWM disable consumption.
 * Public for host-test visibility.
 */
void App_Service1ms(AppContext *app, uint32_t now);

/*
 * 10ms background: state machine service.
 * Public for host-test visibility.
 */
void App_Service10ms(AppContext *app, uint32_t now);

/*
 * 100ms background: diagnostics snapshot, telemetry readout,
 * scheduler-miss and SPI-timeout fault detection.
 * Public for host-test visibility.
 */
void App_Service100ms(AppContext *app, Scheduler *sched, uint32_t now);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
