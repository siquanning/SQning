#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- System States ---- */
typedef enum
{
    SYSTEM_STATE_BOOT     = 0,
    SYSTEM_STATE_INIT     = 1,
    SYSTEM_STATE_STANDBY  = 2,
    SYSTEM_STATE_RUN      = 3,
    SYSTEM_STATE_FAULT    = 4
} SystemState;

/* ---- Fault Codes ---- */
typedef enum
{
    FAULT_NONE                       = 0,

    /* Hardware fast faults */
    FAULT_HW_TZ_TRIP                 = 1,
    FAULT_HW_ADC_STUCK_HIGH          = 2,
    FAULT_HW_ADC_STUCK_LOW           = 3,
    FAULT_HW_PLL_LOCK_LOST           = 4,

    /* Software control faults */
    FAULT_SW_OUTPUT_SATURATION       = 10,
    FAULT_SW_INPUT_OUT_OF_RANGE      = 11,
    FAULT_SW_CONTROL_INVALID         = 12,

    /* System / maintenance faults */
    FAULT_SYS_SELF_TEST_FAIL         = 20,
    FAULT_SYS_PARAM_REJECTED         = 21,
    FAULT_SYS_SCHEDULER_MISS         = 22,
    FAULT_SYS_ISR_WCET_EXCEEDED      = 23,

    /* Communication faults */
    FAULT_COMM_SPI_TIMEOUT_EXCESSIVE = 30,
    FAULT_COMM_QUEUE_OVERFLOW        = 31
} SystemFault;

/* ---- State Machine Context ---- */
typedef struct
{
    SystemState  state;
    SystemFault  first_fault;
    uint32_t     fault_tick;
    uint32_t     state_entry_tick;
    uint32_t     state_duration_ticks;
    uint32_t     pwm_disable_requested;
} StateMachine;

/* ---- Diagnostic flags ---- */
/* 构建期硬件确认闸关闭 (BOARD_PWM_ADC_HW_CONFIRMED==0) 时置位 —
 * 仅记录 PWM/ADC 硬件未确认, 不表达任何运行时 RUN 状态 */
#define DIAG_FLAG_PWM_ADC_HW_UNCONFIRMED  0x00000001U

/* ---- API ---- */

/* Transition from BOOT → INIT. Call once after Board_Init(). */
void StateMachine_Init(StateMachine *sm, uint32_t now);

/* Periodic service — evaluate state transitions. Called at 10ms. */
void StateMachine_Service(StateMachine *sm, uint32_t now, uint32_t diag_flags);

/*
 * Enter FAULT state. Idempotent — first caller's fault is latched.
 * Callable from ISR or main loop. Never blocks, never formats, never
 * calls communication services.
 *
 * Side effects:
 *   - Sets state = SYSTEM_STATE_FAULT (only if not already FAULT)
 *   - Records first_fault, fault_tick (first call only)
 *   - Sets pwm_disable_requested = 1
 *   - On next StateMachine_Service(), DrvEpwm_Disable() is called
 */
void System_EnterFault(StateMachine *sm, SystemFault fault, uint32_t now);

/*
 * Clear fault and return to STANDBY.
 * Prototype: allowed. Industrial: denied (except comm faults).
 * Returns 1 on success, 0 on denied.
 */
int System_ClearFault(StateMachine *sm);

/*
 * Request transition to RUN state.
 * Returns 1 if preconditions satisfied and transition occurred, 0 otherwise.
 */
int StateMachine_RequestRun(StateMachine *sm, uint32_t now);

/*
 * Request transition to STANDBY from RUN.
 * Returns 1 on success, 0 if not in RUN.
 */
int StateMachine_RequestStandby(StateMachine *sm, uint32_t now);

/* Convenience query: returns 1 if state == RUN. */
int StateMachine_IsRun(const StateMachine *sm);

/* Convenience query: returns 1 if state == STANDBY. */
int StateMachine_IsStandby(const StateMachine *sm);

/* Convenience query: returns 1 if state == FAULT. */
int StateMachine_IsFault(const StateMachine *sm);

/*
 * Atomically consume (read-and-clear) the PWM disable request flag.
 * Returns 1 if a disable was pending; 0 otherwise.
 *
 * Caller must call DrvEpwm_Disable() when this returns 1.
 * Must be called from background context only (not ISR).
 * The read-and-clear is guarded by a critical section on C2000
 * so that a concurrent System_EnterFault from ISR cannot lose a request.
 */
int StateMachine_ConsumePwmDisableRequest(StateMachine *sm);

/*
 * Read diagnostic snapshot: current state, first fault code, fault tick.
 * App layer uses this to populate Diagnostics without touching internal fields.
 */
void StateMachine_GetDiagSnapshot(const StateMachine *sm,
                                  uint16_t *state_out,
                                  uint16_t *fault_code_out,
                                  uint32_t *fault_tick_out);

#ifdef __cplusplus
}
#endif

#endif /* STATE_MACHINE_H */
