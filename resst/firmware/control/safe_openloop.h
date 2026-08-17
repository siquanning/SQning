/* Created by Siquanning */
#ifndef SAFE_OPENLOOP_H
#define SAFE_OPENLOOP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint16_t cmp_value;
    uint16_t valid;
} SafeOpenLoopResult;

/*
 * Map an ADC channel reading to a clamped PWM compare value.
 *
 * Parameters:
 *   raw_adc:         ADC reading (0-4095 valid; >4095 = error/invalid)
 *   tbprd:           PWM period register value (1-65535)
 *   max_duty_permill: Hard upper limit in per-mill (1-1000)
 *   gain_permill:     Target gain as fraction of max_duty (0-1000)
 *
 * Returns:
 *   .valid = 1  → .cmp_value is a valid clamped CMP value
 *   .valid = 0  → raw_adc was out of range; .cmp_value = 0 (safe)
 *
 * Pure function — no hardware dependency, host-testable.
 */
SafeOpenLoopResult SafeOpenLoop_MapChannel(uint16_t raw_adc,
                                           uint16_t tbprd,
                                           uint16_t max_duty_permill,
                                           uint16_t gain_permill);

#ifdef __cplusplus
}
#endif

#endif /* SAFE_OPENLOOP_H */
