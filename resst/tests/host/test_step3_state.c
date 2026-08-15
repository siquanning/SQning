#ifdef __TMS320C28XX__
static int _host_test_placeholder_step3_state;
#else

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "firmware/platform_profile.h"
#include "firmware/app/state_machine.h"

/* Host stub: production implementation drives the external hardware gate. */
void DrvGpio_WriteFaultGate(uint16_t enable)
{
    (void)enable;
}

static int g_failures = 0;

#define FAIL(msg) do { \
    printf("FAIL: %s\n", (msg)); fflush(stdout); \
    g_failures++; \
} while(0)

#define ASSERT_EQ(actual, expected, label) do { \
    unsigned _a = (unsigned)(actual); \
    unsigned _e = (unsigned)(expected); \
    if (_a != _e) { \
        printf("  %s: got %u, expected %u\n", (label), _a, _e); \
        FAIL(label); \
    } \
} while(0)

#define ASSERT_TRUE(cond, label) do { \
    if (!(cond)) { printf("  %s: expected true\n", label); FAIL(label); } \
} while(0)

/* ==================================================================
 * T1: BOOT → INIT → STANDBY → RUN → FAULT normal sequence
 * ================================================================== */
static void test_normal_transitions(void)
{
    StateMachine sm;
    memset(&sm, 0, sizeof(sm));

    /* Init sets BOOT */
    StateMachine_Init(&sm, 100UL);
    ASSERT_EQ(sm.state, SYSTEM_STATE_BOOT, "T1.1: init → BOOT");

    /* Service with no diag error → BOOT→INIT→STANDBY */
    StateMachine_Service(&sm, 200UL, 0UL);
    ASSERT_EQ(sm.state, SYSTEM_STATE_INIT, "T1.2: BOOT→INIT");

    StateMachine_Service(&sm, 300UL, 0UL);
    ASSERT_EQ(sm.state, SYSTEM_STATE_STANDBY, "T1.3: INIT→STANDBY");

    /* Request RUN */
    ASSERT_TRUE(StateMachine_RequestRun(&sm, 400UL) == 1, "T1.4: RequestRun success");
    ASSERT_EQ(sm.state, SYSTEM_STATE_RUN, "T1.5: STANDBY→RUN");

    /* Enter fault */
    System_EnterFault(&sm, FAULT_HW_TZ_TRIP, 500UL);
    ASSERT_EQ(sm.state, SYSTEM_STATE_FAULT, "T1.6: RUN→FAULT");
    ASSERT_EQ(sm.first_fault, FAULT_HW_TZ_TRIP, "T1.7: fault code latched");
    ASSERT_EQ(sm.fault_tick, 500UL, "T1.8: fault tick recorded");
}

/* ==================================================================
 * T2: Fault idempotency — first fault is latched
 * ================================================================== */
static void test_fault_idempotency(void)
{
    StateMachine sm;
    memset(&sm, 0, sizeof(sm));
    StateMachine_Init(&sm, 100UL);
    StateMachine_Service(&sm, 200UL, 0UL);
    StateMachine_Service(&sm, 300UL, 0UL);
    StateMachine_RequestRun(&sm, 400UL);

    System_EnterFault(&sm, FAULT_HW_TZ_TRIP, 500UL);
    ASSERT_EQ(sm.first_fault, FAULT_HW_TZ_TRIP, "T2.1: first fault = TZ_TRIP");

    /* Second fault should be ignored */
    System_EnterFault(&sm, FAULT_SW_CONTROL_INVALID, 600UL);
    ASSERT_EQ(sm.first_fault, FAULT_HW_TZ_TRIP, "T2.2: second fault ignored");
    ASSERT_EQ(sm.fault_tick, 500UL, "T2.3: fault tick unchanged");
}

/* ==================================================================
 * T3: FAULT_NONE is ignored
 * ================================================================== */
static void test_fault_none_ignored(void)
{
    StateMachine sm;
    memset(&sm, 0, sizeof(sm));
    StateMachine_Init(&sm, 100UL);
    StateMachine_Service(&sm, 200UL, 0UL);
    StateMachine_Service(&sm, 300UL, 0UL);
    StateMachine_RequestRun(&sm, 400UL);

    System_EnterFault(&sm, FAULT_NONE, 500UL);
    ASSERT_EQ(sm.state, SYSTEM_STATE_RUN, "T3.1: FAULT_NONE → stay in RUN");
}

/* ==================================================================
 * T4: ClearFault (Prototype) → STANDBY
 * ================================================================== */
static void test_clear_fault_prototype(void)
{
    StateMachine sm;
    memset(&sm, 0, sizeof(sm));
    StateMachine_Init(&sm, 100UL);
    StateMachine_Service(&sm, 200UL, 0UL);
    StateMachine_Service(&sm, 300UL, 0UL);
    StateMachine_RequestRun(&sm, 400UL);

    System_EnterFault(&sm, FAULT_HW_TZ_TRIP, 500UL);
    ASSERT_EQ(sm.state, SYSTEM_STATE_FAULT, "T4.1: entered FAULT");

    ASSERT_TRUE(System_ClearFault(&sm) == 1, "T4.2: Prototype ClearFault returns 1");
    ASSERT_EQ(sm.state, SYSTEM_STATE_STANDBY, "T4.3: FAULT→STANDBY");
    ASSERT_EQ(sm.first_fault, FAULT_NONE, "T4.4: fault cleared");
}

/* ==================================================================
 * T5: ClearFault denied when not in FAULT
 * ================================================================== */
static void test_clear_fault_when_not_fault(void)
{
    StateMachine sm;
    memset(&sm, 0, sizeof(sm));
    StateMachine_Init(&sm, 100UL);
    StateMachine_Service(&sm, 200UL, 0UL);
    StateMachine_Service(&sm, 300UL, 0UL);

    ASSERT_TRUE(System_ClearFault(&sm) == 0, "T5.1: ClearFault denied in STANDBY");
}

/* ==================================================================
 * T6: RequestRun denied from non-STANDBY states
 * ================================================================== */
static void test_run_preconditions(void)
{
    StateMachine sm;
    memset(&sm, 0, sizeof(sm));
    StateMachine_Init(&sm, 100UL);

    /* From BOOT — denied */
    ASSERT_TRUE(StateMachine_RequestRun(&sm, 200UL) == 0, "T6.1: RequestRun from BOOT denied");

    StateMachine_Service(&sm, 200UL, 0UL);
    /* From INIT — denied */
    ASSERT_TRUE(StateMachine_RequestRun(&sm, 300UL) == 0, "T6.2: RequestRun from INIT denied");

    StateMachine_Service(&sm, 300UL, 0UL);
    /* From STANDBY — allowed */
    ASSERT_TRUE(StateMachine_RequestRun(&sm, 400UL) == 1, "T6.3: RequestRun from STANDBY allowed");
}

/* ==================================================================
 * T7: RequestStandby from RUN
 * ================================================================== */
static void test_request_standby(void)
{
    StateMachine sm;
    memset(&sm, 0, sizeof(sm));
    StateMachine_Init(&sm, 100UL);
    StateMachine_Service(&sm, 200UL, 0UL);
    StateMachine_Service(&sm, 300UL, 0UL);
    StateMachine_RequestRun(&sm, 400UL);

    ASSERT_TRUE(StateMachine_RequestStandby(&sm, 500UL) == 1, "T7.1: Standby success");
    ASSERT_EQ(sm.state, SYSTEM_STATE_STANDBY, "T7.2: RUN→STANDBY");

    /* RequestStandby from STANDBY → denied */
    ASSERT_TRUE(StateMachine_RequestStandby(&sm, 600UL) == 0, "T7.3: Standby from STANDBY denied");
}

/* ==================================================================
 * T8: pwm_disable_requested flag set on fault
 * ================================================================== */
static void test_pwm_disable_flag(void)
{
    StateMachine sm;
    memset(&sm, 0, sizeof(sm));
    StateMachine_Init(&sm, 100UL);
    StateMachine_Service(&sm, 200UL, 0UL);
    StateMachine_Service(&sm, 300UL, 0UL);
    StateMachine_RequestRun(&sm, 400UL);

    ASSERT_EQ(sm.pwm_disable_requested, 0UL, "T8.1: no disable before fault");

    System_EnterFault(&sm, FAULT_SW_INPUT_OUT_OF_RANGE, 500UL);
    ASSERT_EQ(sm.pwm_disable_requested, 1UL, "T8.2: pwm_disable_requested=1 after fault");
}

/* ==================================================================
 * T9: NULL pointer guards
 * ================================================================== */
static void test_null_guards(void)
{
    System_EnterFault(((StateMachine *)0), FAULT_HW_TZ_TRIP, 100UL);
    System_ClearFault(((StateMachine *)0));
    StateMachine_RequestRun(((StateMachine *)0), 100UL);
    StateMachine_RequestStandby(((StateMachine *)0), 100UL);
    /* All null-guarded — reaching here is pass */
}

int main(void)
{
    printf("=== Step 3 State Machine Host Tests ===\n\n");

    test_normal_transitions();
    test_fault_idempotency();
    test_fault_none_ignored();
    test_clear_fault_prototype();
    test_clear_fault_when_not_fault();
    test_run_preconditions();
    test_request_standby();
    test_pwm_disable_flag();
    test_null_guards();

    printf("\n=== %s ===\n", (g_failures == 0) ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return (g_failures > 0) ? 1 : 0;
}

#endif /* !__TMS320C28XX__ */
