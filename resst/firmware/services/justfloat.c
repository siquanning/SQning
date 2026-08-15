#include "DSP2833x_Device.h"
#include "firmware/bsp/board_config.h"
#include "firmware/services/justfloat.h"
#include "firmware/control/control_global.h"
#include "firmware/control/control_closedloop.h"
#include "firmware/services/measurement.h"
#include "firmware/drivers/drv_sci.h"

#if BOARD_DEBUG_JUSTFLOAT_ENABLE

/*
 * 帧缓冲: 8ch × 4B + 4B 帧尾 = 36 字节。
 * C28x 的 char 是 16 位，stdint.h 无 uint8_t — 用 uint16_t 数组，
 * 每个元素只存一个字节值 (0~255)，DrvSci_SendByte 内部 & 0xFF 截低 8 位。
 */
static uint16_t s_frame[JUSTFLOAT_CH_COUNT * 4U + 4U];

volatile uint16_t g_debug_view = BOARD_DEBUG_VIEW_DEFAULT;
volatile uint16_t g_debug_phase = CTRL_TEST_PHASE_A;

void JustFloat_GetChannels(uint16_t view, float *ch)
{
    PLL_State pll_copy;
    MeasurementSample measurement_copy;
    float alpha_copy;

    if (ch == ((float *)0)) return;

    if ((view != DEBUG_VIEW_PLL) && (view != DEBUG_VIEW_CLOSEDLOOP)) {
        view = BOARD_DEBUG_VIEW_DEFAULT;
    }

    if (view == DEBUG_VIEW_PLL) {
        DINT;
        pll_copy         = g_pll;
        measurement_copy = g_measurement;
        alpha_copy       = g_switch_alpha;
        EINT;

        ch[0] = measurement_copy.vac_v[0];
        ch[1] = measurement_copy.vac_v[1];
        ch[2] = measurement_copy.vac_v[2];
        ch[3] = pll_copy.theta;
        ch[4] = pll_copy.freq;
        ch[5] = pll_copy.vq;
        ch[6] = pll_copy.vmag;
        ch[7] = alpha_copy;
    } else {
        uint16_t phase = (ClosedLoop_GetActiveRunMode() == CTRL_RUN_MODE_THREE_PHASE)
                       ? g_debug_phase : ClosedLoop_GetActivePhase();
        uint16_t index = (ClosedLoop_IsValidTestPhase(phase) != 0U)
                       ? phase - CTRL_TEST_PHASE_A : 0U;
        PhaseClosedLoopState s;
        DINT;
        s = g_phase_ctrl[index];
        EINT;
        ch[0] = s.vac; ch[1] = s.iac; ch[2] = s.iref; ch[3] = s.vdc_avg;
        ch[4] = s.vdc_ref_ramp; ch[5] = s.iamp; ch[6] = s.m;
        ch[7] = s.theta_phase;
    }
}

void JustFloat_Send(const float *ch, uint16_t ch_count)
{
    uint16_t len = 0U;
    uint16_t i;

    if (ch_count > JUSTFLOAT_CH_COUNT) {
        ch_count = JUSTFLOAT_CH_COUNT;
    }

    /* float32 小端逐字节拆解 (C28x 本身小端，直接取字节) */
    for (i = 0U; i < ch_count; ++i) {
        const uint16_t *p = (const uint16_t *)&ch[i];
        s_frame[len++] = p[0] & 0x00FFU;
        s_frame[len++] = p[0] >> 8;
        s_frame[len++] = p[1] & 0x00FFU;
        s_frame[len++] = p[1] >> 8;
    }

    /* 帧尾 00 00 80 7F */
    s_frame[len++] = 0x00U;
    s_frame[len++] = 0x00U;
    s_frame[len++] = 0x80U;
    s_frame[len++] = 0x7FU;

    for (i = 0U; i < len; ++i) {
        DrvSci_SendByte(s_frame[i]);
    }
}

/*
 * 1ms tick — 每4次调用发一帧，输出周期4ms（250Hz）。
 * 230400波特下36字节帧在线时间约1.56ms；16字节TX FIFO吸收首段，
 * 前台实际等待约0.87ms。调用点位于1ms安全任务末尾，不延迟本拍故障封锁。
 */
void JustFloat_Service(void)
{
    static uint16_t s_div = 0U;

    if (++s_div < 4U) {
        return;
    }
    s_div = 0U;

    {
        float ch[JUSTFLOAT_CH_COUNT];
        JustFloat_GetChannels(g_debug_view, ch);
        JustFloat_Send(ch, JUSTFLOAT_CH_COUNT);
    }
}

#endif /* BOARD_DEBUG_JUSTFLOAT_ENABLE */
