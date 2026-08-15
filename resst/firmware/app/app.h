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
 *
 * On return the system is in STANDBY (no auto RUN). GPIO21 保持型按钮的
 * 消抖稳定电平即运行请求: 上电 restart_inhibit=1, 必须先观察到稳定 0,
 * 之后 0→1 才由 RunSupervisor 请求 RUN 并释放 PWM。
 */
void App_Init(AppContext *app);

/*
 * App_RunForever — infinite main loop.
 *
 * Background rate groups:
 *   Foreground: drain SCI RX queue, service SpiBridge, service Indicator
 *   1ms:        parameter commit service, PWM disable consumption (FAULT 快速路径)
 *   10ms:       RunSupervisor 启停裁决 + state machine service
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
 * 1ms background: parameter commit + PWM disable consumption
 * (FAULT 快速路径: PWM_BlockOutput + GPIO20 灭)。
 * Public for host-test visibility.
 */
void App_Service1ms(AppContext *app, uint32_t now);

/*
 * 10ms background: RunSupervisor 启停裁决 + state machine service.
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
