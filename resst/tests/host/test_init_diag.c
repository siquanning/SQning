#ifdef __TMS320C28XX__
static int _host_test_placeholder_init_diag;
#else

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "firmware/platform_profile.h"
#include "firmware/app/state_machine.h"
#include "firmware/app/param_manager.h"

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
 * T1: INIT → STANDBY with diag_flags MSB clear (normal path)
 *
 * StateMachine_Service(diag_flags) transitions INIT→STANDBY when
 * (diag_flags & 0x80000000UL) == 0 — i.e. no self-test failure.
 * DIAG_FLAG_PWM_ADC_HW_UNCONFIRMED (0x00000001) does NOT block this.
 * ================================================================== */
static void test_init_to_standby_no_fault(void)
{
    StateMachine sm;
    memset(&sm, 0, sizeof(sm));

    StateMachine_Init(&sm, 100UL);
    ASSERT_EQ(sm.state, SYSTEM_STATE_BOOT, "T1.1: init → BOOT");

    /* First call: BOOT → INIT (regardless of diag_flags) */
    StateMachine_Service(&sm, 200UL, 0UL);
    ASSERT_EQ(sm.state, SYSTEM_STATE_INIT, "T1.2: BOOT → INIT (diag=0)");

    /* Second call: INIT → STANDBY because MSB is clear */
    StateMachine_Service(&sm, 300UL, 0UL);
    ASSERT_EQ(sm.state, SYSTEM_STATE_STANDBY, "T1.3: INIT → STANDBY (MSB clear)");

    /* Verify diag_flags with PWM_ADC_HW_UNCONFIRMED bit does not block transition */
    memset(&sm, 0, sizeof(sm));
    StateMachine_Init(&sm, 100UL);
    StateMachine_Service(&sm, 200UL, 0x00000001UL); /* HW_UNCONFIRMED set */
    ASSERT_EQ(sm.state, SYSTEM_STATE_INIT, "T1.4: BOOT→INIT (with HW_UNCONFIRMED flag)");
    StateMachine_Service(&sm, 300UL, 0x00000001UL);
    ASSERT_EQ(sm.state, SYSTEM_STATE_STANDBY, "T1.5: INIT→STANDBY (HW_UNCONFIRMED does not block)");
}

/* ==================================================================
 * T2: INIT stays in INIT when diag_flags MSB is set (self-test fail)
 *
 * When 0x80000000 is set, the INIT→STANDBY transition is blocked.
 * The state machine remains in INIT until the flag is cleared.
 * ================================================================== */
static void test_init_blocked_by_self_test_fail(void)
{
    StateMachine sm;
    memset(&sm, 0, sizeof(sm));

    StateMachine_Init(&sm, 100UL);
    StateMachine_Service(&sm, 200UL, 0UL);          /* BOOT → INIT */
    ASSERT_EQ(sm.state, SYSTEM_STATE_INIT, "T2.1: in INIT");

    /* 0x80000000 set → transition blocked, stays in INIT */
    StateMachine_Service(&sm, 300UL, 0x80000000UL);
    ASSERT_EQ(sm.state, SYSTEM_STATE_INIT, "T2.2: stays INIT (MSB set)");

    /* Still blocked */
    StateMachine_Service(&sm, 400UL, 0x80000000UL);
    ASSERT_EQ(sm.state, SYSTEM_STATE_INIT, "T2.3: still INIT (MSB set)");

    /* Once flag is cleared, can transition */
    StateMachine_Service(&sm, 500UL, 0UL);
    ASSERT_EQ(sm.state, SYSTEM_STATE_STANDBY, "T2.4: INIT→STANDBY (MSB cleared)");
}

/* ==================================================================
 * T3: StateMachine_ConsumePwmDisableRequest — read-and-clear
 * ================================================================== */
static void test_consume_pwm_disable(void)
{
    StateMachine sm;
    memset(&sm, 0, sizeof(sm));

    StateMachine_Init(&sm, 100UL);
    StateMachine_Service(&sm, 200UL, 0UL);
    StateMachine_Service(&sm, 300UL, 0UL);
    StateMachine_RequestRun(&sm, 400UL);

    /* No disable pending */
    ASSERT_TRUE(StateMachine_ConsumePwmDisableRequest(&sm) == 0,
                "T3.1: consume returns 0 when no request");

    /* Enter fault → pwm_disable_requested = 1 */
    System_EnterFault(&sm, FAULT_HW_TZ_TRIP, 500UL);
    ASSERT_TRUE(StateMachine_ConsumePwmDisableRequest(&sm) == 1,
                "T3.2: consume returns 1 after fault");

    /* Flag is cleared after consume */
    ASSERT_TRUE(StateMachine_ConsumePwmDisableRequest(&sm) == 0,
                "T3.3: second consume returns 0 (flag cleared)");
}

/* ==================================================================
 * T4: StateMachine_GetDiagSnapshot does not touch internal fields directly
 * ================================================================== */
static void test_get_diag_snapshot(void)
{
    StateMachine sm;
    uint16_t state, fault_code;
    uint32_t fault_tick;

    memset(&sm, 0, sizeof(sm));
    StateMachine_Init(&sm, 100UL);
    StateMachine_Service(&sm, 200UL, 0UL);
    StateMachine_Service(&sm, 300UL, 0UL);
    StateMachine_RequestRun(&sm, 400UL);
    System_EnterFault(&sm, FAULT_HW_ADC_STUCK_HIGH, 500UL);

    StateMachine_GetDiagSnapshot(&sm, &state, &fault_code, &fault_tick);
    ASSERT_EQ(state, SYSTEM_STATE_FAULT, "T4.1: state=FAULT");
    ASSERT_EQ(fault_code, FAULT_HW_ADC_STUCK_HIGH, "T4.2: fault_code=ADC_STUCK_HIGH");
    ASSERT_EQ(fault_tick, 500UL, "T4.3: fault_tick=500");

    /* NULL output pointers are safe */
    StateMachine_GetDiagSnapshot(&sm, ((uint16_t *)0), &fault_code, &fault_tick);
    ASSERT_EQ(fault_code, FAULT_HW_ADC_STUCK_HIGH, "T4.4: NULL state_out OK");
}

/* ==================================================================
 * T5: Param_ServicePendingCommit wraps full commit lifecycle
 * ================================================================== */
static void test_service_pending_commit(void)
{
    ParamManager pm;
    ControlParams new_params;

    Param_Init(&pm, 1250U);

    /* No commit requested → returns OK */
    ASSERT_EQ(Param_ServicePendingCommit(&pm), PARAM_COMMIT_OK,
              "T5.1: no request → OK");

    /* Prepare valid params and request commit */
    memcpy(&new_params, &pm.active, sizeof(ControlParams));
    new_params.version = 3U;
    new_params.m_permill[0] = 300;

    Param_SubmitPending(&pm, &new_params);
    Param_RequestCommit(&pm);

    /* Service: should commit successfully */
    ASSERT_EQ(Param_ServicePendingCommit(&pm), PARAM_COMMIT_OK,
              "T5.2: valid commit → OK");
    ASSERT_EQ(pm.commit_count, 1UL, "T5.3: commit_count=1");
    ASSERT_EQ(pm.active.m_permill[0], 300U, "T5.4: active updated");

    /* Submit invalid params */
    new_params.version = 4U;
    new_params.m_permill[0] = 981;  /* Invalid */
    Param_SubmitPending(&pm, &new_params);
    Param_RequestCommit(&pm);

    /* Service: should reject */
    ASSERT_EQ(Param_ServicePendingCommit(&pm), PARAM_COMMIT_REJECTED,
              "T5.5: invalid commit → REJECTED");
    ASSERT_EQ(pm.reject_count, 1UL, "T5.6: reject_count=1");
    ASSERT_EQ(pm.last_reject_reason, PARAM_REJECT_M_RANGE,
              "T5.7: reject reason=M_RANGE");
}

/* ==================================================================
 * T6: Param_GetDiagSnapshot reads without touching internals
 * ================================================================== */
static void test_param_get_diag(void)
{
    ParamManager pm;
    uint32_t commits, rejects;
    uint16_t reason;

    Param_Init(&pm, 1250U);

    Param_GetDiagSnapshot(&pm, &commits, &rejects, &reason);
    ASSERT_EQ(commits, 0UL, "T6.1: commits=0");
    ASSERT_EQ(rejects, 0UL, "T6.2: rejects=0");
    ASSERT_EQ(reason, PARAM_REJECT_NONE, "T6.3: reason=NONE");

    /* NULL output pointers are safe */
    Param_GetDiagSnapshot(&pm, ((uint32_t *)0), &rejects, ((uint16_t *)0));
    ASSERT_EQ(rejects, 0UL, "T6.4: NULL outputs OK");
}

/* ==================================================================
 * T7: NULL guards on new APIs
 * ================================================================== */
static void test_null_guards(void)
{
    /* StateMachine */
    StateMachine_ConsumePwmDisableRequest(((StateMachine *)0));
    StateMachine_GetDiagSnapshot(((StateMachine *)0),
                                 ((uint16_t *)0), ((uint16_t *)0), ((uint32_t *)0));

    /* Param */
    Param_ServicePendingCommit(((ParamManager *)0));
    Param_GetDiagSnapshot(((ParamManager *)0),
                          ((uint32_t *)0), ((uint32_t *)0), ((uint16_t *)0));
}

int main(void)
{
    printf("=== Init/Diag API Host Tests (main.c refactor) ===\n\n");

    test_init_to_standby_no_fault();
    test_init_blocked_by_self_test_fail();
    test_consume_pwm_disable();
    test_get_diag_snapshot();
    test_service_pending_commit();
    test_param_get_diag();
    test_null_guards();

    printf("\n=== %s ===\n", (g_failures == 0) ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return (g_failures > 0) ? 1 : 0;
}

#endif /* !__TMS320C28XX__ */
