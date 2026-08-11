#include "firmware/control/safe_openloop.h"

SafeOpenLoopResult SafeOpenLoop_MapChannel(uint16_t raw_adc,
                                           uint16_t tbprd,
                                           uint16_t max_duty_permill,
                                           uint16_t gain_permill)
{
    SafeOpenLoopResult result;
    uint32_t cmp;
    uint32_t duty_limit;
    uint32_t target_permill;

    result.cmp_value = 0U;
    result.valid = 0U;

    /* Guard: invalid ADC input */
    if (raw_adc > 4095U)
    {
        return result;
    }

    /* Guard: invalid TBPRD */
    if (tbprd == 0U)
    {
        return result;
    }

    /* Clamp gain: 0-1000 per-mill */
    if (gain_permill > 1000U)
    {
        gain_permill = 1000U;
    }

    /* Clamp max_duty: 0-1000 per-mill */
    if (max_duty_permill > 1000U)
    {
        max_duty_permill = 1000U;
    }

    /* Linear mapping: raw → per-mill duty */
    /* duty_permill = (raw * gain_permill) / 4095 */
    target_permill = ((uint32_t)raw_adc * (uint32_t)gain_permill) / 4095U;

    /* Apply hard max_duty clamp */
    if (target_permill > (uint32_t)max_duty_permill)
    {
        target_permill = (uint32_t)max_duty_permill;
    }

    /* Convert per-mill → CMP value */
    duty_limit = ((uint32_t)tbprd * (uint32_t)max_duty_permill) / 1000U;
    cmp = ((uint32_t)tbprd * target_permill) / 1000U;

    /* Clamp to [0, duty_limit] */
    if (cmp > duty_limit)
    {
        cmp = duty_limit;
    }

    result.cmp_value = (uint16_t)cmp;
    result.valid = 1U;
    return result;
}
