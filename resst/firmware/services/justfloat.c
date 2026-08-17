/* Created by Siquanning */
#include <math.h>
#include "firmware/bsp/board_config.h"
#include "firmware/services/justfloat.h"
#include "firmware/app/debug_snapshot.h"
#include "firmware/drivers/drv_sci.h"
#include "firmware/drivers/drv_interrupt.h"

#define JUSTFLOAT_TWO_PI_3  (2.094395102f)

/*
 * 运行期调试变量（CCS Expressions 可在线修改；SCI-C RX 协议也可修改）。
 * 仅在 JustFloat 发送关闭时这些变量仍必须存在，因此定义在功能宏之外。
 */
volatile uint16_t g_jf_view   = BOARD_DEBUG_VIEW_DEFAULT;
volatile uint16_t g_jf_phase  = 0U;
volatile uint16_t g_jf_enable = BOARD_JUSTFLOAT_ENABLE_DEFAULT;
volatile uint16_t g_jf_lite_mode = BOARD_JUSTFLOAT_LITE_MODE_DEFAULT;

volatile uint32_t g_jf_sent_count = 0UL;
volatile uint32_t g_jf_drop_count  = 0UL;
volatile uint32_t g_waveform_produced_count = 0UL;
volatile uint32_t g_waveform_sent_count = 0UL;
volatile uint32_t g_waveform_queue_overflow_count = 0UL;
volatile uint16_t g_waveform_queue_max_depth = 0U;

#if BOARD_DEBUG_JUSTFLOAT_ENABLE

/*
 * 帧缓冲: ch × 4B + 4B 帧尾。
 * C28x 的 char 是 16 位，stdint.h 无 uint8_t — 用 uint16_t 数组，
 * 每个元素只存一个字节值 (0~255)，写入 SCI TX FIFO 时 & 0xFF 截低 8 位。
 */
#define JUSTFLOAT_FRAME_LEN   (JUSTFLOAT_CH_COUNT * 4U + 4U)

static uint16_t s_tx_buf[JUSTFLOAT_FRAME_LEN];
#if !BOARD_DEBUG_WAVEFORM_LITE
static uint16_t s_frame[JUSTFLOAT_FRAME_LEN];
#endif
static uint16_t s_tx_busy   = 0U;   /* 1 = 当前帧正在写入 FIFO / 线上 */
static uint16_t s_tx_pos    = 0U;
static uint16_t s_tx_len    = 0U;
static uint16_t s_tx_paused = 0U;   /* 1 = 协议独占 TX，队列仍可入队 */

#if BOARD_DEBUG_WAVEFORM_LITE
#if (JUSTFLOAT_WAVE_QUEUE_LEN & (JUSTFLOAT_WAVE_QUEUE_LEN - 1U)) != 0U
#error "JUSTFLOAT_WAVE_QUEUE_LEN must be a power of 2"
#endif
#define JF_WAVE_Q_MASK        (JUSTFLOAT_WAVE_QUEUE_LEN - 1U)

typedef struct {
    float ch[JUSTFLOAT_CH_COUNT];
} JfWaveSample;

static JfWaveSample s_q[JUSTFLOAT_WAVE_QUEUE_LEN];
static uint16_t s_q_wr = 0U;
static uint16_t s_q_rd = 0U;
#endif

#if BOARD_DEBUG_WAVEFORM_LITE
static void jf_lite_ch_from_snap(volatile const DebugSnapshot *s, float *ch)
{
    if (g_jf_lite_mode == JUSTFLOAT_LITE_MODE_VDC) {
        ch[0] = s->vdc[0];
        ch[1] = s->vdc[1];
        ch[2] = s->vdc[2];
        ch[3] = s->vdc[3];
        ch[4] = s->vdc[4];
        ch[5] = s->vdc[5];
        return;
    }
    if (g_jf_lite_mode == JUSTFLOAT_LITE_MODE_DQ) {
        ch[0] = s->id_ref;
        ch[1] = s->id;
        ch[2] = s->iq;
        ch[3] = s->vdc_avg;
        ch[4] = s->vdc_ref_ramp;
        ch[5] = s->m_final;
        return;
    }
    ch[0] = s->vac[0];
    ch[1] = s->vac[1];
    ch[2] = s->vac[2];
    ch[3] = s->iac[0];
    ch[4] = s->iac[1];
    ch[5] = s->iac[2];
}

static uint16_t jf_queue_empty(void)
{
    return (s_q_wr == s_q_rd) ? 1U : 0U;
}

static uint16_t jf_pop_to_txbuf(void)
{
    uint16_t i;
    uint16_t len = 0U;
    const float *ch;

    if (jf_queue_empty() != 0U) {
        return 0U;
    }
    ch = s_q[s_q_rd].ch;
    for (i = 0U; i < JUSTFLOAT_CH_COUNT; ++i) {
        const uint16_t *p = (const uint16_t *)&ch[i];
        s_tx_buf[len++] = p[0] & 0x00FFU;
        s_tx_buf[len++] = p[0] >> 8;
        s_tx_buf[len++] = p[1] & 0x00FFU;
        s_tx_buf[len++] = p[1] >> 8;
    }
    s_tx_buf[len++] = 0x00U;
    s_tx_buf[len++] = 0x00U;
    s_tx_buf[len++] = 0x80U;
    s_tx_buf[len++] = 0x7FU;
    s_tx_len = len;
    s_tx_pos = 0U;
    s_q_rd = (uint16_t)((s_q_rd + 1U) & JF_WAVE_Q_MASK);
    return 1U;
}
#endif /* BOARD_DEBUG_WAVEFORM_LITE */

static void jf_fill_fifo(void)
{
    uint16_t free_n = DrvSci_GetTxFifoFree();
    uint16_t rem = (uint16_t)(s_tx_len - s_tx_pos);
    uint16_t n = free_n;
    uint16_t i;

    if (n > rem) {
        n = rem;
    }
    for (i = 0U; i < n; ++i) {
        DrvSci_TxPutByte(s_tx_buf[s_tx_pos++]);
    }
}

static void jf_arm_tx(void)
{
    if (s_tx_paused != 0U) {
        return;
    }
    DrvSci_ClearTxIntFlag();
    DrvSci_TxIntEnable(1U);
}

/*
 * JustFloat_GetChannels — 从统一 DebugSnapshot 读取一帧通道数据。
 * 快照在 20kHz 控制 ISR 末尾更新；此处 DINT 保护下整帧拷贝。
 * 禁止从 20kHz ISR 调用（RestoreGlobal=EINT）。
 */
void JustFloat_GetChannels(uint16_t view, float *ch)
{
    DebugSnapshot s;

    if (ch == ((float *)0)) return;

    DrvInterrupt_DisableGlobal();
    s = g_dbg_snap;
    DrvInterrupt_RestoreGlobal();

#if BOARD_DEBUG_WAVEFORM_LITE
    (void)view;
    jf_lite_ch_from_snap(&s, ch);
#else
    if (view > DEBUG_VIEW_MAX) {
        view = BOARD_DEBUG_VIEW_DEFAULT;
    }

    switch (view)
    {
    case DEBUG_VIEW_PLL:            /* 0: 实测 Vac + PLL 生成三相跟随波 + freq + vq */
        ch[0] = s.vac[0];
        ch[1] = s.vac[1];
        ch[2] = s.vac[2];
        ch[3] = s.pll_vmag * cosf(s.pll_theta);
        ch[4] = s.pll_vmag * cosf(s.pll_theta - JUSTFLOAT_TWO_PI_3);
        ch[5] = s.pll_vmag * cosf(s.pll_theta + JUSTFLOAT_TWO_PI_3);
        ch[6] = s.pll_freq;
        ch[7] = s.pll_vq;
        break;

    case DEBUG_VIEW_PLL_INTERNAL:   /* 1: PLL 内部 dq/mag/freq/lock */
        ch[0] = s.vac[0];
        ch[1] = s.vac[1];
        ch[2] = s.vac[2];
        ch[3] = s.pll_vd;
        ch[4] = s.pll_vq;
        ch[5] = s.pll_vmag;
        ch[6] = s.pll_freq;
        ch[7] = (float)s.pll_lock;
        break;

    case DEBUG_VIEW_ACQ:            /* 2: 采样实际值（交流三相电流优先） */
        {
            uint16_t oi = s.obs_idx;
            ch[0] = s.iac[0];
            ch[1] = s.iac[1];
            ch[2] = s.iac[2];
            ch[3] = s.vac[0];
            ch[4] = s.vac[1];
            ch[5] = s.vac[2];
            ch[6] = s.vdc[2U * oi];
            ch[7] = s.vdc[2U * oi + 1U];
        }
        break;

    case DEBUG_VIEW_VDC_OVERVIEW:   /* 3: 六路 Vdc 总览 + 当前相 avg/ref */
        ch[0] = s.vdc[0];
        ch[1] = s.vdc[1];
        ch[2] = s.vdc[2];
        ch[3] = s.vdc[3];
        ch[4] = s.vdc[4];
        ch[5] = s.vdc[5];
        ch[6] = s.vdc_avg;
        ch[7] = s.vdc_ref_ramp;
        break;

    case DEBUG_VIEW_VDC_LOOP:       /* 4: Vdc 外环（当前观测相） */
        {
            uint16_t oi = s.obs_idx;
            ch[0] = s.vdc[2U * oi];
            ch[1] = s.vdc[2U * oi + 1U];
            ch[2] = s.vdc_avg;
            ch[3] = s.vdc_ref_ramp;
            ch[4] = s.vdc_err;
            ch[5] = s.iamp;
            ch[6] = s.vdc_integral;
            ch[7] = s.iamp_lim;
        }
        break;

    case DEBUG_VIEW_DQ_LOOP:        /* 5: dq 电流内环（当前观测相） */
        ch[0] = s.id_ref;
        ch[1] = s.id;
        ch[2] = s.id_err;
        ch[3] = s.iq_ref;
        ch[4] = s.iq;
        ch[5] = s.iq_err;
        ch[6] = s.vd_ctrl;
        ch[7] = s.vq_ctrl;
        break;

    case DEBUG_VIEW_PWM_SAFETY:     /* 6: PWM / 安全链 */
        ch[0] = s.m_final;
        ch[1] = (s.force_left != 0U) ? 65535.0f : (float)s.cmp_left;
        ch[2] = (s.force_right != 0U) ? 65535.0f : (float)s.cmp_right;
        ch[3] = (float)s.uni_polarity;
        ch[4] = (float)s.gpio30;
        ch[5] = (float)s.active_phase;
        ch[6] = (float)s.tz_status;
        ch[7] = (float)s.state;
        break;

    case DEBUG_VIEW_RUNSTATE:       /* 7: 启停状态 */
        ch[0] = (float)s.run_request;
        ch[1] = (float)s.state;
        ch[2] = (float)s.active_phase;
        ch[3] = (float)s.pll_lock;
        ch[4] = (float)s.gpio30;
        ch[5] = (float)s.gpio42;
        ch[6] = (float)s.gpio44;
        ch[7] = (float)s.fault;
        break;

    case DEBUG_VIEW_OVERVIEW:       /* 8: 综合运行 */
        ch[0] = s.vdc_avg;
        ch[1] = s.vac[s.obs_idx];
        ch[2] = s.iac[s.obs_idx];
        ch[3] = s.id_ref;
        ch[4] = s.m_final;
        ch[5] = s.pll_freq;
        ch[6] = (float)s.state;
        ch[7] = (float)s.fault;
        break;

    case DEBUG_VIEW_QSG_DIAG:       /* 9: 虚拟正交轴诊断 */
        ch[0] = s.iac[s.obs_idx];
        ch[1] = s.i_alpha;
        ch[2] = s.i_beta;
        ch[3] = s.theta_phase;
        ch[4] = s.id;
        ch[5] = s.iq;
        ch[6] = s.pll_freq;
        ch[7] = (float)s.active_phase;
        break;

    case DEBUG_VIEW_LINE_V:         /* 10: 线电压 Vab/Vbc/Vca */
        ch[0] = s.vline[0];
        ch[1] = s.vline[1];
        ch[2] = s.vline[2];
        ch[3] = s.pll_vmag;
        ch[4] = s.pll_freq;
        ch[5] = s.pll_vq;
        ch[6] = (float)s.active_phase;
        ch[7] = (float)s.pll_lock;
        break;

    default:
        ch[0] = s.vac[0];
        ch[1] = s.vac[1];
        ch[2] = s.vac[2];
        ch[3] = s.pll_vmag * cosf(s.pll_theta);
        ch[4] = s.pll_vmag * cosf(s.pll_theta - JUSTFLOAT_TWO_PI_3);
        ch[5] = s.pll_vmag * cosf(s.pll_theta + JUSTFLOAT_TWO_PI_3);
        ch[6] = s.pll_freq;
        ch[7] = s.pll_vq;
        break;
    }
#endif
}

void JustFloat_OnSnapshot(void)
{
#if BOARD_DEBUG_WAVEFORM_LITE
    uint16_t next;
    uint16_t depth;

    if (g_jf_enable == 0U) {
        return;
    }

    next = (uint16_t)((s_q_wr + 1U) & JF_WAVE_Q_MASK);
    if (next == s_q_rd) {
        g_waveform_queue_overflow_count++;
        return;
    }

    jf_lite_ch_from_snap(&g_dbg_snap, s_q[s_q_wr].ch);
    s_q_wr = next;
    g_waveform_produced_count++;
    depth = (uint16_t)((s_q_wr - s_q_rd) & JF_WAVE_Q_MASK);
    if (depth > g_waveform_queue_max_depth) {
        g_waveform_queue_max_depth = depth;
    }

    if ((s_tx_busy == 0U) && (s_tx_paused == 0U)) {
        jf_arm_tx();
    }
#else
    /* 完整 VIEW 路径不在快 ISR 入队。 */
#endif
}

void JustFloat_Send(const float *ch, uint16_t ch_count)
{
#if BOARD_DEBUG_WAVEFORM_LITE
    /*
     * Lite 波形只从 1kHz 快照点入队，避免前台 Send 与 ISR 双生产者。
     * 保留符号供旧调用点链接；不写 FIFO、不 busy-drop。
     */
    (void)ch;
    (void)ch_count;
#else
    uint16_t len = 0U;
    uint16_t n;
    uint16_t i;

    if (ch_count > JUSTFLOAT_CH_COUNT) {
        ch_count = JUSTFLOAT_CH_COUNT;
    }

    for (i = 0U; i < ch_count; ++i) {
        const uint16_t *p = (const uint16_t *)&ch[i];
        s_frame[len++] = p[0] & 0x00FFU;
        s_frame[len++] = p[0] >> 8;
        s_frame[len++] = p[1] & 0x00FFU;
        s_frame[len++] = p[1] >> 8;
    }

    s_frame[len++] = 0x00U;
    s_frame[len++] = 0x00U;
    s_frame[len++] = 0x80U;
    s_frame[len++] = 0x7FU;

    if (s_tx_busy != 0U) {
        g_jf_drop_count++;
        return;
    }

    for (i = 0U; i < len; ++i) {
        s_tx_buf[i] = s_frame[i];
    }
    s_tx_len = len;
    s_tx_pos = 0U;
    s_tx_busy = 1U;

    jf_fill_fifo();
    DrvSci_ClearTxIntFlag();
    DrvSci_TxIntEnable(1U);
#endif
}

void JustFloat_Service(void)
{
#if BOARD_DEBUG_WAVEFORM_LITE
    if (g_jf_enable == 0U) {
        s_q_wr = 0U;
        s_q_rd = 0U;
        s_tx_busy = 0U;
        s_tx_paused = 0U;
        s_tx_pos = 0U;
        s_tx_len = 0U;
        DrvSci_TxIntEnable(0U);
        return;
    }

    if (s_tx_paused != 0U) {
        s_tx_paused = 0U;
    }

    if ((s_tx_busy == 0U) && (jf_queue_empty() == 0U)) {
        jf_arm_tx();
    }
#else
    static uint16_t s_div = 0U;

    if (g_jf_enable == 0U) {
        s_div = 0U;
        return;
    }

    if (s_tx_paused != 0U) {
        s_tx_paused = 0U;
        s_div = 0U;
        return;
    }

    if (++s_div < BOARD_JUSTFLOAT_PERIOD_MS) {
        return;
    }
    s_div = 0U;

    {
        float ch[JUSTFLOAT_CH_COUNT];
        JustFloat_GetChannels(g_jf_view, ch);
        JustFloat_Send(ch, JUSTFLOAT_CH_COUNT);
    }
#endif
}

#ifdef __TMS320C28XX__
__interrupt void JustFloat_ScicTxIsr(void)
#else
void JustFloat_ScicTxIsr(void)
#endif
{
#if BOARD_DEBUG_WAVEFORM_LITE
    for (;;) {
        if (s_tx_busy == 0U) {
            if ((s_tx_paused != 0U) || (jf_pop_to_txbuf() == 0U)) {
                DrvSci_TxIntEnable(0U);
                break;
            }
            s_tx_busy = 1U;
        }

        jf_fill_fifo();

        if (s_tx_pos < s_tx_len) {
            break;
        }

        s_tx_busy = 0U;
        g_waveform_sent_count++;
        g_jf_sent_count++;
    }
#else
    if (s_tx_pos < s_tx_len) {
        jf_fill_fifo();
        if (s_tx_pos >= s_tx_len) {
            s_tx_busy = 0U;
            DrvSci_TxIntEnable(0U);
            g_jf_sent_count++;
            g_waveform_sent_count++;
        }
    } else {
        s_tx_busy = 0U;
        DrvSci_TxIntEnable(0U);
        g_jf_sent_count++;
        g_waveform_sent_count++;
    }
#endif
    DrvSci_ClearTxIntFlag();
    DrvInterrupt_AckGroup8();
}

void JustFloat_TxYieldForProtocol(void)
{
    while (s_tx_busy != 0U) {
        /* 等待当前帧发完（TX ISR 清 busy）；不是 while(FIFO)。 */
    }
    s_tx_paused = 1U;
}

#endif /* BOARD_DEBUG_JUSTFLOAT_ENABLE */
