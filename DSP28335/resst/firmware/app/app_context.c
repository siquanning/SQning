#include "firmware/app/app_context.h"
#include "firmware/bsp/board_config.h"

void AppContext_Init(AppContext *context)
{
    SciRxQueue_Init(&context->sci_rx_queue);
    SpiBridge_Init(&context->spi_bridge);

    /* 控制上下文: m[0..2]=0 → 50% 零调制点, 其余由 active 参数同步 */
    context->control.m_permill[0]         = 0;
    context->control.m_permill[1]         = 0;
    context->control.m_permill[2]         = 0;
    context->control.control_mode         = 0U;
    context->control.tbprd                = (uint16_t)BOARD_PWM_TBPRD;
    context->control.adc_safe_min         = 1U;
    context->control.adc_safe_max         = 4094U;
    context->control.fault_thresh_adc_stuck = 10U;
    context->control.step_count           = 0U;
    context->control.stuck_ctr_high       = 0U;
    context->control.stuck_ctr_low        = 0U;
    context->control.acc_error            = 0UL;

    /* State machine */
    StateMachine_Init(&context->state_machine, 0UL);

    /* 参数管理器: m 默认 0, 50% 零调制点 */
    Param_Init(&context->param_manager,
               (uint16_t)BOARD_PWM_TBPRD);

    /* Telemetry double-buffer */
    Telemetry_Init(&context->telemetry);
}
