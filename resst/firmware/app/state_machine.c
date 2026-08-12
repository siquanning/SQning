#include "firmware/app/state_machine.h"
#include "firmware/platform_profile.h"
#include "firmware/drivers/drv_gpio.h"

/*
 * Critical-section helpers for C2000 background vs. ISR concurrency.
 * On C2000, __disable_interrupts() / __enable_interrupts() are compiler
 * intrinsics that manipulate the INTM bit in ST1.
 * On host (single-threaded tests), no protection is needed.
 */
#ifdef __TMS320C28XX__
#define CRIT_ENTER()  __disable_interrupts()
#define CRIT_LEAVE()  __enable_interrupts()
#else
#define CRIT_ENTER()  ((void)0)
#define CRIT_LEAVE()  ((void)0)
#endif

#if !PLATFORM_CAP_FAULT_QUICK_RESET
/* Only needed in Industrial builds for comm-fault clearing */
static int IsCommFault(SystemFault fault)
{
    return (fault >= FAULT_COMM_SPI_TIMEOUT_EXCESSIVE &&
            fault <= FAULT_COMM_QUEUE_OVERFLOW);
}
#endif

void StateMachine_Init(StateMachine *sm, uint32_t now)
{
    if (sm == ((StateMachine *)0)) return;

    sm->state                = SYSTEM_STATE_BOOT;
    sm->first_fault          = FAULT_NONE;
    sm->fault_tick           = 0UL;
    sm->state_entry_tick     = now;
    sm->state_duration_ticks = 0UL;
    sm->logical_run          = 0UL;
    sm->pwm_disable_requested = 0UL;
}

void StateMachine_Service(StateMachine *sm, uint32_t now, uint32_t diag_flags)
{
    if (sm == ((StateMachine *)0)) return;

    sm->state_duration_ticks = now - sm->state_entry_tick;

    switch (sm->state)
    {
    case SYSTEM_STATE_BOOT:
        /* BOOT → INIT: automatic after StateMachine_Init called */
        sm->state = SYSTEM_STATE_INIT;
        sm->state_entry_tick = now;
        break;

    case SYSTEM_STATE_INIT:
        /* INIT → STANDBY: automatic if no fault detected */
        if ((diag_flags & 0x80000000UL) == 0UL)
        {
            sm->state = SYSTEM_STATE_STANDBY;
            sm->state_entry_tick = now;
        }
        break;

    case SYSTEM_STATE_STANDBY:
        /* Awaiting explicit RUN request — no automatic transition */
        break;

    case SYSTEM_STATE_RUN:
        /* RUN maintained until fault or explicit STOP */
        break;

    case SYSTEM_STATE_FAULT:
        /*
         * PWM disable is requested at fault entry, but if System_EnterFault
         * was called from ISR context, the actual DrvEpwm_Disable call
         * happens here (background). The flag is set regardless.
         * AQCSFRC force LOW was already set by DrvEpwm_Disable if
         * called from main loop; if from ISR, the request flag is
         * consumed here and PWM is disabled from background.
         */
        break;

    default:
        sm->state = SYSTEM_STATE_FAULT;
        sm->first_fault = FAULT_SYS_SELF_TEST_FAIL;
        sm->fault_tick = now;
        sm->state_entry_tick = now;
        break;
    }
}

void System_EnterFault(StateMachine *sm, SystemFault fault, uint32_t now)
{
    if (sm == ((StateMachine *)0)) return;
    if (fault == FAULT_NONE) return;

    /* Idempotent: first caller's fault is latched */
    if (sm->state == SYSTEM_STATE_FAULT)
    {
        return;
    }

    /*
     * Pull FAULT_GATE LOW immediately — ISR-safe (no EALLOW required).
     * This must happen BEFORE the state change so the CPLD blocks all
     * gates at the earliest possible moment, even if called from ISR.
     */
    DrvGpio_WriteFaultGate(0U);

    sm->state                = SYSTEM_STATE_FAULT;
    sm->first_fault          = fault;
    sm->fault_tick           = now;
    sm->state_entry_tick     = now;
    sm->pwm_disable_requested = 1UL;
}

int System_ClearFault(StateMachine *sm)
{
    if (sm == ((StateMachine *)0)) return 0;
    if (sm->state != SYSTEM_STATE_FAULT) return 0;

#if PLATFORM_CAP_FAULT_QUICK_RESET
    sm->state                = SYSTEM_STATE_STANDBY;
    sm->first_fault          = FAULT_NONE;
    sm->fault_tick           = 0UL;
    sm->state_entry_tick     = 0UL;
    sm->pwm_disable_requested = 0UL;
    return 1;
#else
    /* Industrial: only comm faults are clearable */
    if (IsCommFault(sm->first_fault))
    {
        sm->state                = SYSTEM_STATE_STANDBY;
        sm->first_fault          = FAULT_NONE;
        sm->fault_tick           = 0UL;
        sm->state_entry_tick     = 0UL;
        sm->pwm_disable_requested = 0UL;
        return 1;
    }
    return 0;
#endif
}

int StateMachine_RequestRun(StateMachine *sm, uint32_t now)
{
    if (sm == ((StateMachine *)0)) return 0;
    if (sm->state != SYSTEM_STATE_STANDBY) return 0;

    sm->state = SYSTEM_STATE_RUN;
    sm->state_entry_tick = now;
    sm->logical_run = 1UL;
    return 1;
}

int StateMachine_RequestStandby(StateMachine *sm, uint32_t now)
{
    if (sm == ((StateMachine *)0)) return 0;
    if (sm->state != SYSTEM_STATE_RUN) return 0;

    sm->state = SYSTEM_STATE_STANDBY;
    sm->state_entry_tick = now;
    sm->logical_run = 0UL;
    return 1;
}

int StateMachine_IsRun(const StateMachine *sm)
{
    if (sm == ((StateMachine *)0)) return 0;
    return (sm->state == SYSTEM_STATE_RUN) ? 1 : 0;
}

int StateMachine_IsFault(const StateMachine *sm)
{
    if (sm == ((StateMachine *)0)) return 0;
    return (sm->state == SYSTEM_STATE_FAULT) ? 1 : 0;
}

int StateMachine_ConsumePwmDisableRequest(StateMachine *sm)
{
    uint32_t was_requested;

    if (sm == ((StateMachine *)0)) return 0;

    CRIT_ENTER();
    was_requested = sm->pwm_disable_requested;
    sm->pwm_disable_requested = 0UL;
    CRIT_LEAVE();

    return (was_requested != 0UL) ? 1 : 0;
}

void StateMachine_GetDiagSnapshot(const StateMachine *sm,
                                  uint16_t *state_out,
                                  uint16_t *fault_code_out,
                                  uint32_t *fault_tick_out)
{
    if (sm == ((const StateMachine *)0)) return;

    if (state_out      != ((uint16_t *)0)) *state_out      = (uint16_t)sm->state;
    if (fault_code_out != ((uint16_t *)0)) *fault_code_out = (uint16_t)sm->first_fault;
    if (fault_tick_out != ((uint32_t *)0)) *fault_tick_out = sm->fault_tick;
}
