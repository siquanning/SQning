#include "firmware/app/param_manager.h"
#include "firmware/platform_profile.h"
#include "firmware/bsp/board_config.h"
#include "firmware/control/control_common.h"
#include <string.h>

void Param_Init(ParamManager *pm, uint16_t tbprd)
{
    uint16_t i;

    if (pm == ((ParamManager *)0)) return;

    memset(&pm->pending, 0, sizeof(ControlParams));
    memset(&pm->active,  0, sizeof(ControlParams));

    pm->active.version             = 1U;
    for (i = 0U; i < 3U; i++)
    {
        pm->active.m_permill[i]    = 0;           /* m=0 → 50% 零调制点 */
    }
    pm->active.control_mode        = 0U;
    pm->active.tbprd               = tbprd;
    pm->active.adc_safe_min        = 1U;
    pm->active.adc_safe_max        = 4094U;
    pm->active.fault_thresh_adc_stuck    = 10U;
    pm->active.fault_thresh_sched_miss   = 100U;
    pm->active.fault_thresh_spi_timeout  = 50U;

    pm->pending = pm->active;
    pm->pending.version = 2U;

    pm->commit_requested   = 0UL;
    pm->commit_count       = 0UL;
    pm->reject_count       = 0UL;
    pm->last_reject_reason = PARAM_REJECT_NONE;
    pm->reserved_pad       = 0U;
}

void Param_SubmitPending(ParamManager *pm,
                         const ControlParams *new_params)
{
    if (pm          == ((ParamManager *)0)) return;
    if (new_params  == ((const ControlParams *)0)) return;

    memcpy(&pm->pending, new_params, sizeof(ControlParams));
}

void Param_RequestCommit(ParamManager *pm)
{
    if (pm == ((ParamManager *)0)) return;
    pm->commit_requested = 1UL;
}

uint16_t Param_Validate(const ControlParams *params,
                        const ControlParams *current_active)
{
    uint16_t i;

    if (params          == ((const ControlParams *)0)) return PARAM_REJECT_VERSION;
    if (current_active  == ((const ControlParams *)0)) return PARAM_REJECT_VERSION;

    /* 版本号必须单调递增 */
    if (params->version <= current_active->version)
    {
        return PARAM_REJECT_VERSION;
    }

    /* m 范围: [-980, +980] per-mill */
    for (i = 0U; i < 3U; i++)
    {
        if (params->m_permill[i] < MOD_M_PERMILL_MIN ||
            params->m_permill[i] > MOD_M_PERMILL_MAX)
        {
            return PARAM_REJECT_M_RANGE;
        }
    }

    /* 控制模式: 仅支持 passthrough (0) */
    if (params->control_mode != 0U)
    {
        return PARAM_REJECT_CONTROL_MODE;
    }

    /* TBPRD 范围 */
    if (params->tbprd == 0U || params->tbprd > 65535U)
    {
        return PARAM_REJECT_TBPRD_RANGE;
    }

    /* ADC safe_min < safe_max */
    if (params->adc_safe_min >= params->adc_safe_max)
    {
        return PARAM_REJECT_ADC_RANGE_ORDER;
    }

    /* ADC safe_max ≤ 4095 */
    if (params->adc_safe_max > 4095U)
    {
        return PARAM_REJECT_ADC_MAX_EXCEED;
    }

    /* 故障阈值必须 > 0 */
    if (params->fault_thresh_adc_stuck == 0U)
    {
        return PARAM_REJECT_THRESH_ZERO;
    }

    return PARAM_REJECT_NONE;
}

int Param_CheckPendingCommit(ParamManager *pm)
{
    uint16_t reason;

    if (pm == ((ParamManager *)0)) return 0;
    if (pm->commit_requested == 0UL) return 0;

    reason = Param_Validate(&pm->pending, &pm->active);

    if (reason != PARAM_REJECT_NONE)
    {
        pm->reject_count++;
        pm->last_reject_reason = reason;
        pm->commit_requested = 0UL;
        return 0;
    }

    memcpy(&pm->active, &pm->pending, sizeof(ControlParams));

    pm->commit_count++;
    pm->commit_requested = 0UL;
    return 1;
}

void Param_ReadActive(const ParamManager *pm,
                      ControlParams *out)
{
    if (pm  == ((const ParamManager *)0)) return;
    if (out == ((ControlParams *)0)) return;

    uint16_t v1, v2;
    int iter;

    for (iter = 0; iter < 2; iter++)
    {
        v1 = pm->active.version;
        memcpy(out, &pm->active, sizeof(ControlParams));
        v2 = pm->active.version;

        if (v1 == v2)
        {
            return;
        }
    }

    memcpy(out, &pm->active, sizeof(ControlParams));
}

int Param_ServicePendingCommit(ParamManager *pm)
{
    if (pm == ((ParamManager *)0)) return PARAM_COMMIT_OK;

    if (pm->commit_requested == 0UL) return PARAM_COMMIT_OK;

    return Param_CheckPendingCommit(pm) ? PARAM_COMMIT_OK : PARAM_COMMIT_REJECTED;
}

void Param_GetDiagSnapshot(const ParamManager *pm,
                           uint32_t *commit_count_out,
                           uint32_t *reject_count_out,
                           uint16_t *last_reject_reason_out)
{
    if (pm == ((const ParamManager *)0)) return;

    if (commit_count_out      != ((uint32_t *)0)) *commit_count_out      = pm->commit_count;
    if (reject_count_out      != ((uint32_t *)0)) *reject_count_out      = pm->reject_count;
    if (last_reject_reason_out != ((uint16_t *)0)) *last_reject_reason_out = pm->last_reject_reason;
}
