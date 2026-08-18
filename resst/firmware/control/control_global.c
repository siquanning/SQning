/* Created by Siquanning */
#include "firmware/control/control_global.h"

PLL_State g_pll;
volatile float g_pll_input_vabc[3] = {0.0f, 0.0f, 0.0f};
volatile float g_pll_input_vline[3] = {0.0f, 0.0f, 0.0f};

volatile uint16_t g_pll_switch_req      = 0U;
volatile float    g_switch_alpha        = 0.0f;
volatile float    g_switch_phase_err_deg = 0.0f;
