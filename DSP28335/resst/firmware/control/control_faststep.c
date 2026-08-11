#include "firmware/control/control_faststep.h"
#include "firmware/bsp/board_config.h"

#define ADC_RAW_MIN_SAFE        1U
#define ADC_RAW_MAX_SAFE        4094U

#define CONTROL_FAULT_INPUT_RANGE     1U
#define CONTROL_FAULT_ADC_STUCK_HIGH  2U
#define CONTROL_FAULT_ADC_STUCK_LOW   3U

static void Control_InitOutput(ControlOutput *output)
{
    uint16_t i;
    for (i = 0U; i < 3U; i++)
    {
        output->cmpa[i]    = 0U;
        output->cmpb[i]    = 0U;
        output->force_a[i] = 0U;
        output->force_b[i] = 0U;
    }
    output->valid          = 0U;
    output->fault_code     = 0U;
    output->fault_asserted = 0U;
}

/*
 * Clamped-unipolar PWM modulation for one H-bridge phase.
 *
 *   m > 0 → left=1,  right=PWM(1-m)
 *   m < 0 → left=PWM(1-|m|), right=1
 *   m = 0 → left=1,  right=1  (zero-voltage state)
 *
 * CMP clamps apply only to the chopping half-bridge.
 * The clamped half-bridge uses AQCSFRC force-HIGH (not CMP).
 */
void Control_ComputeModulation(int16_t m_permill, uint16_t tbprd,
                               PhasePwmCommand *cmd)
{
    int32_t m;
    uint32_t duty;
    uint32_t cmp;

    if (cmd == ((PhasePwmCommand *)0))
        return;

    m = (int32_t)m_permill;
    if (m < MOD_M_PERMILL_MIN) m = MOD_M_PERMILL_MIN;
    if (m > MOD_M_PERMILL_MAX) m = MOD_M_PERMILL_MAX;

    cmd->left.cmp        = 0U;
    cmd->left.force_high = 0U;
    cmd->right.cmp        = 0U;
    cmd->right.force_high = 0U;

    if (m > 0)
    {
        cmd->left.force_high = 1U;

        duty = 1000U - (uint32_t)m;
        cmp  = ((uint32_t)tbprd * duty) / 1000U;

        if (cmp < (uint32_t)BOARD_MODULATION_CMP_MIN)
            cmp = (uint32_t)BOARD_MODULATION_CMP_MIN;
        if (cmp > (uint32_t)BOARD_MODULATION_CMP_MAX)
            cmp = (uint32_t)BOARD_MODULATION_CMP_MAX;

        cmd->right.cmp = (uint16_t)cmp;
    }
    else if (m < 0)
    {
        duty = (uint32_t)(1000L + m);   /* m < 0, e.g. m=-200 → duty=800 */
        cmp  = ((uint32_t)tbprd * duty) / 1000U;

        if (cmp < (uint32_t)BOARD_MODULATION_CMP_MIN)
            cmp = (uint32_t)BOARD_MODULATION_CMP_MIN;
        if (cmp > (uint32_t)BOARD_MODULATION_CMP_MAX)
            cmp = (uint32_t)BOARD_MODULATION_CMP_MAX;

        cmd->left.cmp = (uint16_t)cmp;

        cmd->right.force_high = 1U;
    }
    else
    {
        /* m == 0: both half-bridges clamped HIGH (zero-voltage state) */
        cmd->left.force_high  = 1U;
        cmd->right.force_high = 1U;
    }
}

void Control_FastStep(ControlContext *context,
                      const ControlInput *input,
                      ControlOutput *output)
{
    uint16_t fault = 0U;
    uint16_t all_valid = 1U;
    uint16_t i;

    if ((context == ((ControlContext *)0)) ||
        (input    == ((const ControlInput *)0)) ||
        (output   == ((ControlOutput *)0)))
    {
        return;
    }

    Control_InitOutput(output);
    context->step_count++;

    /* ---- ADC 安全范围校验 ---- */
    if (input->adc_raw[0] < context->adc_safe_min ||
        input->adc_raw[0] > context->adc_safe_max)
    {
        all_valid = 0U;
        fault = CONTROL_FAULT_INPUT_RANGE;
    }

    if (input->adc_raw[1] < context->adc_safe_min ||
        input->adc_raw[1] > context->adc_safe_max)
    {
        all_valid = 0U;
        fault = CONTROL_FAULT_INPUT_RANGE;
    }

    /* ---- ADC 卡死检测 ---- */
    if (input->adc_raw[0] >= ADC_RAW_MAX_SAFE)
    {
        context->stuck_ctr_high++;
    }
    else
    {
        context->stuck_ctr_high = 0U;
    }

    if (input->adc_raw[0] <= ADC_RAW_MIN_SAFE)
    {
        context->stuck_ctr_low++;
    }
    else
    {
        context->stuck_ctr_low = 0U;
    }

    if (context->fault_thresh_adc_stuck > 0U)
    {
        if (context->stuck_ctr_high >= context->fault_thresh_adc_stuck)
        {
            all_valid = 0U;
            fault = CONTROL_FAULT_ADC_STUCK_HIGH;
        }
        if (context->stuck_ctr_low >= context->fault_thresh_adc_stuck)
        {
            all_valid = 0U;
            fault = CONTROL_FAULT_ADC_STUCK_LOW;
        }
    }

    /*
     * Per-phase m → PhasePwmCommand, then unpack to legacy ControlOutput.
     *
     * Legacy layout (ADC actuator path is DISABLED — not valid for the
     * new 6-ePWM half-bridge mapping):
     *   cmpa[i]    = phase i left  half-bridge CMP
     *   cmpb[i]    = phase i right half-bridge CMP
     *   force_a[i] = phase i left  force_high
     *   force_b[i] = phase i right force_high
     */
    for (i = 0U; i < 3U; i++)
    {
        PhasePwmCommand ph;

        Control_ComputeModulation(context->m_permill[i], context->tbprd, &ph);

        output->cmpa[i]    = ph.left.cmp;
        output->cmpb[i]    = ph.right.cmp;
        output->force_a[i] = ph.left.force_high;
        output->force_b[i] = ph.right.force_high;
    }

    /* ---- 填充输出状态 ---- */
    if (all_valid == 0U)
    {
        for (i = 0U; i < 3U; i++)
        {
            output->cmpa[i]    = 0U;
            output->cmpb[i]    = 0U;
            output->force_a[i] = 0U;
            output->force_b[i] = 0U;
        }
        output->valid          = 0U;
        output->fault_asserted = 1U;
        output->fault_code     = fault;
    }
    else
    {
        output->valid          = 1U;
        output->fault_asserted = 0U;
        output->fault_code     = 0U;
    }
}
