#include <math.h>
#include <stdio.h>
#include "firmware/bsp/board_config.h"
#include "firmware/app/debug_snapshot.h"
#include "firmware/services/justfloat.h"

/* 统一调试快照：本测试直接填充快照验证 VIEW 映射 */
DebugSnapshot g_dbg_snap;

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", (m)); failures++; } } while(0)

/*
 * 模拟 SCI-C TX FIFO（host 测试）:
 *   tx_fifo[]    记录所有写入 SCITXBUF 的字节（DrvSci_TxPutByte）
 *   tx_free      当前可写入槽位数（GetTxFifoFree）
 *   tx_free_cap  每次“UART 发走一批”后恢复的空闲上限（模拟 TXFFIL）
 */
static unsigned tx_fifo[512];
static unsigned tx_fifo_len;
static unsigned tx_int_en;
static unsigned tx_free = 16U;
static unsigned tx_free_cap = 16U;

void DrvSci_TxPutByte(uint16_t byte)
{
    tx_fifo[tx_fifo_len++] = (unsigned)(byte & 0xFFU);
    if (tx_free > 0U) tx_free--;
}
uint16_t DrvSci_GetTxFifoFree(void)  { return (uint16_t)tx_free; }
void DrvSci_TxIntEnable(uint16_t en) { tx_int_en = en ? 1U : 0U; }
void DrvSci_ClearTxIntFlag(void)     { }
void DrvInterrupt_AckGroup8(void)    { }
void DrvInterrupt_DisableGlobal(void) { }
void DrvInterrupt_RestoreGlobal(void) { }

#define TEST_FRAME_LEN (JUSTFLOAT_CH_COUNT * 4U + 4U)

static void tx_reset(void)
{
    tx_fifo_len = 0U;
    tx_int_en = 0U;
    tx_free = 16U;
    tx_free_cap = 16U;
}

static void stats_reset(void)
{
    g_jf_sent_count = 0UL;
    g_jf_drop_count = 0UL;
    g_waveform_produced_count = 0UL;
    g_waveform_sent_count = 0UL;
    g_waveform_queue_overflow_count = 0UL;
    g_waveform_queue_max_depth = 0U;
}

static void jf_reset(void)
{
    g_jf_enable = 0U;
    JustFloat_Service();
    g_jf_enable = 1U;
    stats_reset();
    tx_reset();
}

/* 每次 ISR 前恢复 FIFO 空闲，模拟 UART 已把水位降到 TXFFIL。 */
static void drain_until_idle(unsigned expect_total)
{
    int guard = 0;
    while (tx_int_en != 0U && guard < 256) {
        tx_free = tx_free_cap;
        JustFloat_ScicTxIsr();
        guard++;
    }
    CHECK(tx_fifo_len == expect_total, "queue drained fully");
    CHECK(tx_int_en == 0U, "TX int disabled after queue idle");
}

static int frame_tail_ok(unsigned base)
{
    return tx_fifo[base + 24U] == 0x00U &&
           tx_fifo[base + 25U] == 0x00U &&
           tx_fifo[base + 26U] == 0x80U &&
           tx_fifo[base + 27U] == 0x7FU;
}

#if BOARD_DEBUG_WAVEFORM_LITE
static void test_waveform_lite_modes(void)
{
    float ch[JUSTFLOAT_CH_COUNT];
    uint16_t i;

    for (i = 0U; i < 3U; i++) {
        g_dbg_snap.vac[i] = 10.0f + (float)i;   /* Va/Vb/Vc */
        g_dbg_snap.iac[i] = 20.0f + (float)i;   /* Ia/Ib/Ic */
    }
    for (i = 0U; i < 6U; i++) {
        g_dbg_snap.vdc[i] = 30.0f + (float)i;   /* Vdc1..Vdc6 */
    }

    CHECK(g_jf_lite_mode == BOARD_JUSTFLOAT_LITE_MODE_DEFAULT,
          "lite mode defaults to AC");
    g_jf_lite_mode = JUSTFLOAT_LITE_MODE_AC;
    JustFloat_GetChannels(99U, ch);             /* view 被忽略 */
    CHECK(ch[0]==10.0f && ch[1]==11.0f && ch[2]==12.0f &&
          ch[3]==20.0f && ch[4]==21.0f && ch[5]==22.0f,
          "lite AC: CH1-6 = Va/Vb/Vc/Ia/Ib/Ic");

    g_jf_lite_mode = JUSTFLOAT_LITE_MODE_VDC;
    JustFloat_GetChannels(99U, ch);
    CHECK(ch[0]==30.0f && ch[1]==31.0f && ch[2]==32.0f &&
          ch[3]==33.0f && ch[4]==34.0f && ch[5]==35.0f,
          "lite Vdc: CH1-6 = Vdc1..Vdc6");

    g_dbg_snap.id_ref = 1.0f;
    g_dbg_snap.id = 0.8f;
    g_dbg_snap.iq = 0.1f;
    g_dbg_snap.vdc_avg = 40.0f;
    g_dbg_snap.vdc_ref_ramp = 42.0f;
    g_dbg_snap.m_final = 0.12f;
    g_jf_lite_mode = JUSTFLOAT_LITE_MODE_DQ;
    JustFloat_GetChannels(99U, ch);
    CHECK(ch[0]==1.0f && ch[1]==0.8f && ch[2]==0.1f &&
          ch[3]==40.0f && ch[4]==42.0f && ch[5]==0.12f,
          "lite DQ: CH1-6 = Id_ref/Id/Iq/VdcAvg/VdcRefRamp/m");

    g_jf_lite_mode = JUSTFLOAT_LITE_MODE_MAX + 1U;
    JustFloat_GetChannels(99U, ch);
    CHECK(ch[0]==10.0f && ch[5]==22.0f,
          "invalid lite mode falls back to AC");
    g_jf_lite_mode = BOARD_JUSTFLOAT_LITE_MODE_DEFAULT;
}
#else
static void fill_snapshot(void)
{
    uint16_t i;
    DebugSnapshot *s = &g_dbg_snap;

    for (i = 0U; i < 3U; i++) {
        s->vac[i]     = 10.0f + (float)i;   /* 实测 Va/Vb/Vc */
        s->iac[i]     = 20.0f + (float)i;   /* 换算 Iac */
        s->vac_raw[i] = 1000.0f + (float)i;
        s->iac_raw[i] = 2000.0f + (float)i;
        s->vac_offset[i] = 30.0f + (float)i;
        s->iac_offset[i] = 40.0f + (float)i;
    }
    for (i = 0U; i < 6U; i++) s->vdc[i] = 50.0f + (float)i;

    /* 线电压 = 相电压相减（同一参考点）：Va=10, Vb=11, Vc=12 → -1, -1, 2 */
    s->vline[0] = s->vac[0] - s->vac[1];
    s->vline[1] = s->vac[1] - s->vac[2];
    s->vline[2] = s->vac[2] - s->vac[0];

    s->pll_freq  = 50.0f;
    s->pll_vd    = 60.0f;
    s->pll_vq    = 61.0f;
    s->pll_vmag  = 70.0f;
    s->pll_theta = 0.5f;
    s->pll_lock  = 1U;

    s->obs_idx       = 1U;                  /* 观测 B 相 */
    s->vdc_avg       = 105.0f;
    s->vdc_balance   = 3.0f;
    s->vdc_ref_ramp  = 106.0f;
    s->vdc_integral  = 107.0f;
    s->vdc_err       = 1.0f;
    s->iamp          = 108.0f;
    s->iamp_lim      = 1.0f;
    s->id_ref        = 109.0f;
    s->iq_ref        = 0.0f;
    s->id            = 107.0f;
    s->iq            = 1.0f;
    s->id_err        = 2.0f;
    s->iq_err        = -1.0f;
    s->id_integral   = 110.0f;
    s->iq_integral   = 8.0f;
    s->vd_ctrl       = 111.0f;
    s->vq_ctrl       = 9.0f;
    s->i_alpha       = 113.0f;
    s->i_beta        = 114.0f;
    s->m_raw         = 0.31f;
    s->m_final       = 0.20f;
    s->theta_phase   = 112.0f;

    s->mabc[0] = -100; s->mabc[1] = 200; s->mabc[2] = -300;
    s->uni_polarity = 3U;                    /* bit0+bit1: A/B 左腿钳位 */
    s->cmp_left   = 1500U;
    s->force_left = 1U;
    s->cmp_right  = 2500U;
    s->force_right = 0U;

    s->run_request  = 1U;
    s->state        = 3U;                    /* RUN */
    s->active_phase = 2U;
    s->active_mode  = 1U;
    s->gpio30 = 1U; s->gpio42 = 0U; s->gpio44 = 1U;
    s->tz_status = 4U;                       /* TZFLG OST */
    s->fault = 0U;
}

static void test_view_pll_follow(void)
{
    float ch[JUSTFLOAT_CH_COUNT];
    JustFloat_GetChannels(DEBUG_VIEW_PLL, ch);
    CHECK(ch[0]==10.0f && ch[1]==11.0f && ch[2]==12.0f,
          "VIEW0 CH1-3 = 实测 Va/Vb/Vc");
    CHECK(fabsf(ch[3]-70.0f*cosf(0.5f))<1.0e-6f &&
          fabsf(ch[4]-70.0f*cosf(0.5f-2.094395102f))<1.0e-6f &&
          fabsf(ch[5]-70.0f*cosf(0.5f+2.094395102f))<1.0e-6f,
          "VIEW0 CH4-6 = PLL 三相跟随波(±120°)");
    CHECK(ch[6]==50.0f && ch[7]==61.0f, "VIEW0 CH7/8 = freq/vq");
}

static void test_view_pll_internal(void)
{
    float ch[JUSTFLOAT_CH_COUNT];
    JustFloat_GetChannels(DEBUG_VIEW_PLL_INTERNAL, ch);
    CHECK(ch[0]==10.0f && ch[1]==11.0f && ch[2]==12.0f &&
          ch[3]==60.0f && ch[4]==61.0f && ch[5]==70.0f &&
          ch[6]==50.0f && ch[7]==1.0f,
          "VIEW1 = Va/Vb/Vc + vd/vq/vmag/freq/lock");
}

static void test_view_acq(void)
{
    float ch[JUSTFLOAT_CH_COUNT];
    uint16_t oi = g_dbg_snap.obs_idx;
    JustFloat_GetChannels(DEBUG_VIEW_ACQ, ch);
    CHECK(ch[0]==g_dbg_snap.iac[0] && ch[1]==g_dbg_snap.iac[1] && ch[2]==g_dbg_snap.iac[2] &&
          ch[3]==g_dbg_snap.vac[0] && ch[4]==g_dbg_snap.vac[1] && ch[5]==g_dbg_snap.vac[2] &&
          ch[6]==g_dbg_snap.vdc[2U*oi] && ch[7]==g_dbg_snap.vdc[2U*oi+1U],
          "VIEW2 = Ia/Ib/Ic + Va/Vb/Vc + 当前相 Vdc1/Vdc2 实际值");
}

static void test_view_vdc_overview(void)
{
    float ch[JUSTFLOAT_CH_COUNT];
    JustFloat_GetChannels(DEBUG_VIEW_VDC_OVERVIEW, ch);
    CHECK(ch[0]==50.0f && ch[1]==51.0f && ch[2]==52.0f &&
          ch[3]==53.0f && ch[4]==54.0f && ch[5]==55.0f &&
          ch[6]==105.0f && ch[7]==106.0f,
          "VIEW3 = Vdc1..6 + 当前相 VdcAvg + VdcRefRamp");
}

static void test_view_vdc_loop(void)
{
    float ch[JUSTFLOAT_CH_COUNT];
    uint16_t oi = g_dbg_snap.obs_idx;
    JustFloat_GetChannels(DEBUG_VIEW_VDC_LOOP, ch);
    CHECK(ch[0]==g_dbg_snap.vdc[2U*oi] && ch[1]==g_dbg_snap.vdc[2U*oi+1U] &&
          ch[2]==105.0f && ch[3]==106.0f && ch[4]==1.0f &&
          ch[5]==108.0f && ch[6]==107.0f && ch[7]==1.0f,
          "VIEW4 = Vdc1/Vdc2/VdcAvg/VrefRamp/VdcErr/Iamp/Integral/Limit");
}

static void test_view_dq_loop(void)
{
    float ch[JUSTFLOAT_CH_COUNT];
    JustFloat_GetChannels(DEBUG_VIEW_DQ_LOOP, ch);
    CHECK(ch[0]==109.0f && ch[1]==107.0f && ch[2]==2.0f &&
          ch[3]==0.0f && ch[4]==1.0f && ch[5]==-1.0f &&
          ch[6]==111.0f && ch[7]==9.0f,
          "VIEW5 = Id_ref/Id/Id_err/Iq_ref/Iq/Iq_err/Vd_ctrl/Vq_ctrl");
}

static void test_view_qsg_diag(void)
{
    float ch[JUSTFLOAT_CH_COUNT];
    uint16_t oi = g_dbg_snap.obs_idx;
    JustFloat_GetChannels(DEBUG_VIEW_QSG_DIAG, ch);
    CHECK(ch[0]==g_dbg_snap.iac[oi] && ch[1]==113.0f && ch[2]==114.0f &&
          ch[3]==112.0f && ch[4]==107.0f && ch[5]==1.0f &&
          ch[6]==50.0f && ch[7]==2.0f,
          "VIEW9 = Iac/Ialpha/Ibeta/theta_phase/Id/Iq/freq/activePhase");
}

static void test_view_pwm_safety(void)
{
    float ch[JUSTFLOAT_CH_COUNT];
    JustFloat_GetChannels(DEBUG_VIEW_PWM_SAFETY, ch);
    CHECK(ch[0]==0.20f && ch[1]==65535.0f && ch[2]==2500.0f &&
          ch[3]==3.0f && ch[4]==1.0f && ch[5]==2.0f &&
          ch[6]==4.0f && ch[7]==3.0f,
          "VIEW6 = m_final/左桥CMP(force哨兵)/右桥CMP/UNI/GPIO30/phase/TZ/state");
}

static void test_view_runstate(void)
{
    float ch[JUSTFLOAT_CH_COUNT];
    JustFloat_GetChannels(DEBUG_VIEW_RUNSTATE, ch);
    CHECK(ch[0]==1.0f && ch[1]==3.0f && ch[2]==2.0f && ch[3]==1.0f &&
          ch[4]==1.0f && ch[5]==0.0f && ch[6]==1.0f && ch[7]==0.0f,
          "VIEW7 = runReq/state/activePhase/lock/GPIO30/42/44/fault");
}

static void test_view_overview(void)
{
    float ch[JUSTFLOAT_CH_COUNT];
    uint16_t oi = g_dbg_snap.obs_idx;
    JustFloat_GetChannels(DEBUG_VIEW_OVERVIEW, ch);
    CHECK(ch[0]==105.0f && ch[1]==g_dbg_snap.vac[oi] &&
          ch[2]==g_dbg_snap.iac[oi] && ch[3]==109.0f &&
          ch[4]==0.20f && ch[5]==50.0f && ch[6]==3.0f && ch[7]==0.0f,
          "VIEW8 = VdcAvg/Vac/Iac/Iref/m_final/freq/state/fault");
}

static void test_view_line_v(void)
{
    float ch[JUSTFLOAT_CH_COUNT];
    JustFloat_GetChannels(DEBUG_VIEW_LINE_V, ch);
    CHECK(ch[0]==-1.0f && ch[1]==-1.0f && ch[2]==2.0f &&
          ch[3]==70.0f && ch[4]==50.0f && ch[5]==61.0f &&
          ch[6]==2.0f && ch[7]==1.0f,
          "VIEW10 = Vab/Vbc/Vca + vmag/freq/vq/activePhase/pllLock");
    CHECK(fabsf(ch[0]+ch[1]+ch[2]) < 1e-6f, "Vab+Vbc+Vca ≈ 0 (telescoping)");
    /* 平衡三相抽查：Va=100, Vb=-50, Vc=-50 → Vab=150, Vbc=0, Vca=-150 */
    g_dbg_snap.vac[0]=100.0f; g_dbg_snap.vac[1]=-50.0f; g_dbg_snap.vac[2]=-50.0f;
    g_dbg_snap.vline[0]=150.0f; g_dbg_snap.vline[1]=0.0f; g_dbg_snap.vline[2]=-150.0f;
    JustFloat_GetChannels(DEBUG_VIEW_LINE_V, ch);
    CHECK(ch[0]==150.0f && fabsf(ch[0]+ch[1]+ch[2])<1e-6f,
          "balanced snapshot: Vab=150, sum≈0");
}

static void test_invalid_view_fallback(void)
{
    float ch[JUSTFLOAT_CH_COUNT];
    JustFloat_GetChannels(99U, ch);
    CHECK(ch[0]==10.0f && ch[7]==61.0f,
          "invalid view falls back to default PLL view");
}
#endif /* BOARD_DEBUG_WAVEFORM_LITE */

#if BOARD_DEBUG_WAVEFORM_LITE
static void test_wave_queue(void)
{
    unsigned i;
    unsigned usable = JUSTFLOAT_WAVE_QUEUE_LEN - 1U;

    jf_reset();
    g_jf_enable = 0U;
    for (i = 0U; i < 8U; i++) {
        JustFloat_OnSnapshot();
        JustFloat_Service();
    }
    CHECK(tx_fifo_len == 0U && g_waveform_produced_count == 0UL,
          "disabled: no produce, no TX");

    jf_reset();
    JustFloat_OnSnapshot();
    CHECK(tx_fifo_len == 0U && tx_int_en == 1U &&
          g_waveform_produced_count == 1UL && g_waveform_sent_count == 0UL,
          "OnSnapshot enqueues and arms TX, does not write FIFO");
    CHECK(g_jf_drop_count == 0UL, "first sample is not busy-dropped");

    /* 第二拍在 TX ISR 之前入队，而不是 drop */
    JustFloat_OnSnapshot();
    CHECK(g_waveform_produced_count == 2UL && g_jf_drop_count == 0UL &&
          g_waveform_queue_overflow_count == 0UL &&
          g_waveform_queue_max_depth == 2U,
          "busy no longer drops; second sample queued");

    drain_until_idle(TEST_FRAME_LEN * 2U);
    CHECK(g_waveform_sent_count == 2UL && g_jf_sent_count == 2UL,
          "TX ISR drains both frames without waiting Service");
    CHECK(frame_tail_ok(0U) && frame_tail_ok(TEST_FRAME_LEN),
          "both frames have JustFloat tail 00 00 80 7F");

    /* 帧完成后队列仍有数据时，同一 ISR 路径立即开始下一帧（上面已覆盖）。
     * 协议暂停：入队保留，不发送，Service kick 后恢复。 */
    JustFloat_TxYieldForProtocol();
    tx_reset();
    stats_reset();
    /* 保留队列空、pause=1 */
    g_dbg_snap.vac[0] = 1.5f;
    JustFloat_OnSnapshot();
    CHECK(tx_fifo_len == 0U && g_waveform_produced_count == 1UL,
          "paused: sample queued, no FIFO write");
    JustFloat_Service();
    drain_until_idle(TEST_FRAME_LEN);
    CHECK(g_waveform_sent_count == 1UL, "Service kick resumes TX after yield");

    /* 满队列 overflow，不覆盖旧样本 */
    jf_reset();
    for (i = 0U; i < usable; i++) {
        JustFloat_OnSnapshot();
    }
    CHECK(g_waveform_produced_count == (uint32_t)usable &&
          g_waveform_queue_overflow_count == 0UL &&
          g_waveform_queue_max_depth == (uint16_t)usable,
          "queue accepts LEN-1 frames");
    JustFloat_OnSnapshot();
    CHECK(g_waveform_produced_count == (uint32_t)usable &&
          g_waveform_queue_overflow_count == 1UL,
          "16th produce overflows instead of overwriting");
    drain_until_idle(TEST_FRAME_LEN * usable);
    CHECK(g_waveform_sent_count == (uint32_t)usable,
          "overflowed sample is the only one lost");
}

static void test_fill_by_free(void)
{
    jf_reset();
    tx_free = 8U;
    tx_free_cap = 8U;
    JustFloat_OnSnapshot();
    JustFloat_ScicTxIsr();
    CHECK(tx_fifo_len == 8U && tx_free == 0U,
          "TX ISR writes only GetTxFifoFree() bytes, not a fixed 16");
    drain_until_idle(TEST_FRAME_LEN);
    CHECK(g_waveform_sent_count == 1UL && frame_tail_ok(0U),
          "TXFFIL=8 style refill still completes 28B frame");
}
#else
static void test_send_enable(void)
{
    float ch[JUSTFLOAT_CH_COUNT];
    unsigned i;

    jf_reset();
    g_jf_enable = 0U;
    for (i = 0; i < 8; i++) JustFloat_Service();
    CHECK(tx_fifo_len == 0U, "disabled JustFloat sends no bytes");

    g_jf_enable = 1U;
    JustFloat_GetChannels(g_jf_view, ch);
    JustFloat_Send(ch, JUSTFLOAT_CH_COUNT);
    CHECK(tx_fifo_len == 16U && tx_int_en == 1U && g_jf_sent_count == 0UL,
          "frame A: first fill uses free slots, not yet complete");
    drain_until_idle(TEST_FRAME_LEN);
    CHECK(g_jf_sent_count == 1UL, "sent_count incremented when frame done");
}
#endif

int main(void)
{
#if BOARD_DEBUG_WAVEFORM_LITE
    printf("=== JustFloat Lite Waveform Tests (%uB/frame) ===\n",
           (unsigned)TEST_FRAME_LEN);
    test_waveform_lite_modes();
    test_wave_queue();
    test_fill_by_free();
#else
    printf("=== JustFloat Debug Snapshot View Tests ===\n");
    fill_snapshot();
    test_view_pll_follow();
    test_view_pll_internal();
    test_view_acq();
    test_view_vdc_overview();
    test_view_vdc_loop();
    test_view_dq_loop();
    test_view_qsg_diag();
    test_view_pwm_safety();
    test_view_runstate();
    test_view_overview();
    test_invalid_view_fallback();
    test_view_line_v();
    test_send_enable();
#endif
    printf("=== %s ===\n", failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED");
    return failures ? 1 : 0;
}
