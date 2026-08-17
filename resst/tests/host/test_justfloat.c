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
 *   tx_fifo_len  已写入字节总数
 *   tx_int_en    当前 TXFFIENA 状态（1 = TX FIFO 中断已使能）
 * host 下 JustFloat_ScicTxIsr() 可被直接调用，模拟一次 FIFO 空中断。
 */
static unsigned tx_fifo[128];
static unsigned tx_fifo_len;
static unsigned tx_int_en;

void DrvSci_TxPutByte(uint16_t byte) { tx_fifo[tx_fifo_len++] = (unsigned)(byte & 0xFFU); }
uint16_t DrvSci_GetTxFifoFree(void)  { return 16U; }
void DrvSci_TxIntEnable(uint16_t en) { tx_int_en = en ? 1U : 0U; }
void DrvSci_ClearTxIntFlag(void)     { }
void DrvInterrupt_AckGroup8(void)    { }
void DrvInterrupt_DisableGlobal(void) { }
void DrvInterrupt_RestoreGlobal(void) { }

/* 帧长（lite=28B / full=36B 通用） */
#define TEST_FRAME_LEN (JUSTFLOAT_CH_COUNT * 4U + 4U)

/* 模拟 FIFO 空中断，直到当前帧完成（tx_int_en 被最终 ISR 清 0） */
static void drain_until_frame_done(unsigned expect_total)
{
    int guard = 0;
    while (tx_int_en != 0U && guard < 16) {
        JustFloat_ScicTxIsr();
        guard++;
    }
    CHECK(tx_fifo_len == expect_total, "frame drained fully");
    CHECK(tx_int_en == 0U, "TX int disabled after frame done");
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

    g_jf_lite_mode = JUSTFLOAT_LITE_MODE_VDC + 1U;
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

static void test_send_enable(void)
{
    int i;

    /* 关闭时一字节也不发 */
    tx_fifo_len = 0U; tx_int_en = 0U;
    g_jf_sent_count = 0UL; g_jf_drop_count = 0UL;
    g_jf_enable = 0U;
    for (i = 0; i < 8; i++) JustFloat_Service();
    CHECK(tx_fifo_len == 0U, "disabled JustFloat sends no bytes");

    /* 帧 A：提交首批 16 字节，TX 中断使能，帧未完成不计 sent */
    g_jf_enable = 1U;
    JustFloat_Service();
    CHECK(tx_fifo_len == 16U && tx_int_en == 1U && g_jf_sent_count == 0UL,
          "frame A: first 16 bytes pushed, TX int armed, not yet sent");

    /* 帧 A 未发完时提交 → 丢弃，不阻塞 */
    JustFloat_Service();
    CHECK(tx_fifo_len == 16U && g_jf_drop_count == 1UL && g_jf_sent_count == 0UL,
          "frame while busy is dropped without blocking");

    /* FIFO 空 → ISR 续搬剩余 → 帧完成（sent++、关中断） */
    drain_until_frame_done(TEST_FRAME_LEN);
    CHECK(g_jf_sent_count == 1UL, "sent_count incremented at final FIFO empty");

    /* 帧完成后可再提交 */
    JustFloat_Service();
    CHECK(tx_fifo_len == TEST_FRAME_LEN + 16U, "frame B accepted after frame A done");
    drain_until_frame_done(TEST_FRAME_LEN * 2U);
    CHECK(g_jf_sent_count == 2UL, "frame B complete");

    /* 协议仲裁：YieldForProtocol 等当前帧发完并暂停下一帧，恢复后正常 */
    JustFloat_Service();                          /* 帧 C 提交 */
    drain_until_frame_done(TEST_FRAME_LEN * 3U);
    CHECK(g_jf_sent_count == 3UL, "frame C complete");
    JustFloat_TxYieldForProtocol();               /* busy=0 立即返回并暂停 */
    JustFloat_Service();                          /* 被暂停：跳过一帧 */
    CHECK(tx_fifo_len == 3U * TEST_FRAME_LEN, "yield pauses exactly one frame");
    JustFloat_Service();                          /* 恢复：下一帧正常提交 */
    CHECK(tx_fifo_len == 3U * TEST_FRAME_LEN + 16U, "frame after yield resumes");
}

int main(void)
{
#if BOARD_DEBUG_WAVEFORM_LITE
    printf("=== JustFloat Lite Waveform Tests (%uB/frame) ===\n",
           (unsigned)TEST_FRAME_LEN);
    test_waveform_lite_modes();
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
#endif
    test_send_enable();
    printf("=== %s ===\n", failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED");
    return failures ? 1 : 0;
}
