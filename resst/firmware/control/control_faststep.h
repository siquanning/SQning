#ifndef CONTROL_FASTSTEP_H
#define CONTROL_FASTSTEP_H

#include "firmware/control/control_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Main fast control entry. Called from ADC ISR at PWM frequency.
 *
 * Deterministic, non-blocking, fixed-cost pure function.
 * No TI headers, no register access, no global tick, no dynamic memory.
 *
 * Postconditions:
 *   - output->cmpa, cmpb are clamped per active parameters
 *   - output->valid = 1 iff all inputs validated
 *   - output->fault_asserted = 1 iff a control-level fault is detected
 *   - context->step_count incremented
 */
/*
 * Pure m→PhasePwmCommand modulation for one H-bridge phase.
 * Implements clamped-unipolar PWM:
 *   m > 0 → left=1,  right=PWM(1-m)
 *   m < 0 → left=PWM(1-|m|), right=1
 *   m = 0 → left=1, right=1 (zero-voltage state)
 *
 * Fills one PhasePwmCommand (left + right half-bridge).
 * No ADC validation, no fault handling — shared by Control_FastStep
 * and foreground test/diagnostic paths.
 */
void Control_ComputeModulation(int16_t m_permill, uint16_t tbprd,
                               PhasePwmCommand *cmd);

void Control_FastStep(ControlContext *context,
                      const ControlInput *input,
                      ControlOutput *output);

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_FASTSTEP_H */
