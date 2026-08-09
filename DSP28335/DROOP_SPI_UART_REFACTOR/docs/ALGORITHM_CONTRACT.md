# ALGORITHM_CONTRACT — DROOP_SPI_UART_REFACTOR

## 1. Purpose

This document defines the entry rules and interface contract for all algorithm modules
under `firmware/algorithm/`. No algorithm module may be merged or activated unless it
satisfies all rules in §2 and has passing Host tests per §5.

Currently, **zero algorithm modules exist**. This contract is a forward-looking
specification — it describes what future modules MUST satisfy.

The current DROOP_SPI_UART_REFACTOR is a UART→SPI bridge. It does not implement
PID, droop control, filtering, ADC sampling, or PWM generation.

## 2. Algorithm Entry Rules (Hard Gates)

Every source file under `firmware/algorithm/` MUST pass these checks:

| # | Rule | Verification |
|---|------|-------------|
| 1 | Does NOT `#include` any `DSP2833x_*.h` or `DSP2833x_Device.h` | grep for `DSP2833x` in algorithm/ |
| 2 | Does NOT access any peripheral registers (`xxxRegs`, `EALLOW`, `EDIS`) | grep for `Regs\|EALLOW\|EDIS` |
| 3 | Does NOT call `delay_ms()`, `DrvTimer_*`, or any blocking function | grep for `delay\|DrvTimer\|while.*!=` without timeout |
| 4 | Does NOT read `g_sysTick`, `Timebase_Now()`, or any global tick counter | grep for `tick\|Timebase` |
| 5 | Does NOT depend on `extern` writable globals outside its own context | grep for `extern.*[^const]` |
| 6 | All mutable state is in a caller-owned context struct passed by pointer | manual review |
| 7 | All inputs, outputs, and limits are explicitly bounded and documented | review of context struct |
| 8 | Can be compiled and tested on a PC host (x64/MSVC or gcc) | host_test_build.bat builds algorithm tests |

### 2.1 Rationale for Each Rule

- **Rule 1-2**: Algorithm code must be device-independent. This enables host testing
  and prevents accidental hardware coupling.
- **Rule 3-4**: Algorithm steps must be purely computational — deterministic given
  the same inputs. No blocking, no hidden time dependencies.
- **Rule 5-6**: Explicit context ownership prevents action-at-a-distance bugs.
  Every function's side effects are visible in its signature.
- **Rule 7**: Control algorithms must declare saturation limits, integrator bounds,
  and valid input ranges before implementation.
- **Rule 8**: Host testing is the first line of defense. DSP target bugs found in
  the field are 10× more expensive than bugs caught on a PC.

## 3. Context-Based Interface Pattern

All algorithm modules use caller-owned context structs:

```c
// Example: PID controller (NOT YET IMPLEMENTED)
typedef struct
{
    float kp;           // Proportional gain
    float ki;           // Integral gain (pre-multiplied by dt)
    float kd;           // Derivative gain (pre-multiplied by 1/dt)
    float integrator;   // Accumulated integral term
    float prev_error;   // Previous error for derivative
    float out_min;      // Output lower saturation limit
    float out_max;      // Output upper saturation limit
    float i_min;        // Integrator lower clamp
    float i_max;        // Integrator upper clamp
} PidContext;

void PID_Init(PidContext *ctx, float kp, float ki, float kd,
              float out_min, float out_max);

float PID_Step(PidContext *ctx, float reference, float feedback);
```

### 3.1 Key Design Decisions

- **Gains are pre-scaled**: `ki` already includes `dt * Ki_raw`. The caller (1ms/10ms
  task) computes `ki = Ki_raw * dt` once at init, avoiding a multiply per step.
- **Anti-windup via clamping**: `integrator` is clamped to `[i_min, i_max]`, not
  back-calculated. This is simpler and sufficient for most power-converter loops.
- **No `float` vs `_iq` mandate**: The module interface uses `float` for clarity.
  Migration to IQ math (`_iq20`, etc.) is a separate optimization decision made
  after profiling, not an interface change.

## 4. Task-to-ISR Allocation Table

This table defines where each control function executes. It is prescriptive:
future control modules MUST follow this allocation.

| Function | Execution Context | Deadline | Rationale |
|----------|------------------|----------|-----------|
| ADC result read | ADC EOC ISR | < ADC sample period | Must read before next conversion |
| Fast control (PID, current loop) | ADC EOC ISR or ePWM ISR | < 10 μs | Phase margin depends on loop latency |
| PWM duty update | ePWM ISR | Before next PWM period | Prevents duty-cycle glitches |
| Fast overcurrent / overvoltage | ePWM Trip Zone (TZ) | < 100 ns (hardware) | Hardware protection, no software latency |
| Droop control outer loop | 1 ms task | < 1 ms | Slow outer loop; 1ms is ~60× control period margin |
| Slow fault debounce | 10 ms task | < 10 ms | Must confirm fault persists before shutdown |
| State machine transitions | Main loop or 10 ms task | < 100 ms | Non-time-critical sequencing |
| Diagnostic snapshots | 100 ms task | < 1 s | Operator display rate |
| UART→SPI bridge | Main loop (every iteration) | < 5 ms (SPI timeout) | Already implemented |

### 4.1 Why NOT put everything in ISRs

- ISR code runs with interrupts disabled for its duration → increases ISR latency.
- Complex state machines (SPI bridge, fault sequencing) are harder to debug in ISR context.
- ISR stack is shared; deep call chains risk overflow.
- The F28335 has no nested interrupt priority — a long ISR blocks all others.

### 4.2 ePWM Trip Zone for Fast Protection

The F28335 ePWM Trip Zone (TZ) subsystem can force PWM outputs to a safe state
in hardware, with zero CPU cycles. This is the correct mechanism for:

- Overcurrent (cycle-by-cycle or one-shot)
- Overvoltage
- External fault signals

Software ISRs should NOT be used for sub-microsecond protection.

## 5. Host Test Template for Algorithm Modules

Every algorithm module must include a PC-host test. Template:

```c
// tests/host/test_pid.c
#ifdef __TMS320C28XX__
static int _host_test_placeholder_pid;
#else

#include <stdio.h>
#include <math.h>
#include "../../firmware/algorithm/pid.h"

static int g_failures = 0;

#define FAIL(msg) do { \
    printf("FAIL: %s\n", (msg)); fflush(stdout); \
    g_failures++; \
} while(0)

#define ASSERT_NEAR(actual, expected, epsilon, label) do { \
    float _a = (float)(actual); \
    float _e = (float)(expected); \
    if (fabsf(_a - _e) > (epsilon)) { \
        printf("  %s: got %.6f, expected %.6f\n", (label), _a, _e); \
        FAIL(label); \
    } \
} while(0)

/* Minimum test coverage:
 *   1. Init and step with zero error → output = 0 (or bias)
 *   2. Proportional-only response
 *   3. Integral windup and clamping
 *   4. Output saturation limits
 *   5. Sign convention (positive error → positive correction?)
 */

int main(void)
{
    printf("=== pid host tests ===\n");

    /* ... test cases ... */

    if (g_failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}

#endif /* !__TMS320C28XX__ */
```

## 6. Current Status

| Item | Status |
|------|--------|
| `firmware/algorithm/` directory | Does not exist (no algorithm modules yet) |
| PID implementation | Not created (UART→SPI bridge doesn't need it) |
| Droop control | Not created |
| Filter (IIR/EMA) | Not created |
| Algorithm host tests | 0 tests (no modules to test) |
| Contract document | This file (created 2026-08-09, Step 4B) |

## 7. Future Module Checklist

When adding a new algorithm module (e.g., PID, IIR filter):

1. [ ] Create `firmware/algorithm/<module>.c` and `.h`
2. [ ] Verify: no TI headers, no registers, no delay, no global tick
3. [ ] Context struct with explicit bounds
4. [ ] `Init()` and `Step()` (or equivalent) functions
5. [ ] Create `tests/host/test_<module>.c`
6. [ ] Add to `host_test_build.bat`
7. [ ] Run host tests — all pass
8. [ ] Assign to correct ISR/task per §4 allocation table
9. [ ] Profile WCET on DSP target
10. [ ] Update this document's status table
