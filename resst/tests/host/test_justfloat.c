#include <math.h>
#include <stdio.h>
#include "firmware/bsp/board_config.h"
#include "firmware/app/debug_snapshot.h"
#include "firmware/services/justfloat.h"

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

#define TEST_FRAME_LEN (JUSTFLOAT_CH_COUNT * 4U + 4U)

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

static void test_waveform_lite_modes(void)
{
    float ch[JUSTFLOAT_CH_COUNT];
    uint16_t i;

    for (i = 0U; i < 3U; i++) {
        g_dbg_snap.vac[i] = 10.0f + (float)i;     /* 相电压 */
        g_dbg_snap.vline[i] = 40.0f + (float)i;   /* 线电压 */
        g_dbg_snap.iac[i] = 20.0f + (float)i;
    }
    for (i = 0U; i < 6U; i++) {
        g_dbg_snap.vdc[i] = 30.0f + (float)i;
    }
    g_dbg_snap.pll_vmag = 10.0f;
    g_dbg_snap.pll_theta = 0.0f;

    CHECK(g_jf_lite_mode == BOARD_JUSTFLOAT_LITE_MODE_DEFAULT,
          "lite mode defaults to line+Iac");
    g_jf_lite_mode = JUSTFLOAT_LITE_MODE_LINE;
    JustFloat_GetChannels(ch);
    CHECK(ch[0]==40.0f && ch[1]==41.0f && ch[2]==42.0f &&
          ch[3]==20.0f && ch[4]==21.0f && ch[5]==22.0f && ch[6]==0.0f,
          "lite 0: CH1-6 = Vab/Vbc/Vca/Ia/Ib/Ic, CH7=0");

    g_jf_lite_mode = JUSTFLOAT_LITE_MODE_VDC;
    JustFloat_GetChannels(ch);
    CHECK(ch[0]==30.0f && ch[1]==31.0f && ch[2]==32.0f &&
          ch[3]==33.0f && ch[4]==34.0f && ch[5]==35.0f && ch[6]==0.0f,
          "lite 1: CH1-6 = Vdc1..Vdc6, CH7=0");

    g_jf_lite_mode = JUSTFLOAT_LITE_MODE_PLL;
    JustFloat_GetChannels(ch);
    CHECK(ch[0]==10.0f && ch[1]==11.0f && ch[2]==12.0f,
          "lite 2: CH1-3 = Va/Vb/Vc");
    CHECK(fabsf(ch[3] - 10.0f) < 1.0e-5f &&
          fabsf(ch[4] + 5.0f) < 1.0e-4f &&
          fabsf(ch[5] + 5.0f) < 1.0e-4f &&
          ch[6]==0.0f,
          "lite 2: CH4-6 = vmag·cos(θ±0/120/240°) at θ=0, CH7=0");

    g_dbg_snap.vdc_avg = 50.0f;
    g_dbg_snap.iac_obs = 2.0f;
    g_dbg_snap.vd_ctrl = 3.0f;
    g_dbg_snap.iamp = 1.5f;
    g_dbg_snap.id = 1.0f;
    g_dbg_snap.iq = 0.1f;
    g_dbg_snap.id_ref = 1.2f;
    g_dbg_snap.vq_ctrl = -0.5f;
    g_dbg_snap.m = 0.4f;
    g_jf_lite_mode = JUSTFLOAT_LITE_MODE_DQ;
    JustFloat_GetChannels(ch);
    CHECK(ch[0]==50.0f && ch[1]==2.0f && ch[2]==3.0f && ch[3]==1.5f &&
          ch[4]==1.0f && ch[5]==0.1f && ch[6]==0.4f,
          "lite 3: CH1-7 = VdcAvg/Iac/vd_ctrl/iamp/Id/Iq/m");

    g_jf_lite_mode = JUSTFLOAT_LITE_MODE_DQ + 1U;
    JustFloat_GetChannels(ch);
    CHECK(ch[0]==40.0f && ch[5]==22.0f && ch[6]==0.0f,
          "invalid lite mode falls back to line+Iac");
    g_jf_lite_mode = BOARD_JUSTFLOAT_LITE_MODE_DEFAULT;
}

static void test_send_enable(void)
{
    int i;

    tx_fifo_len = 0U; tx_int_en = 0U;
    g_jf_sent_count = 0UL; g_jf_drop_count = 0UL;
    g_jf_enable = 0U;
    for (i = 0; i < 8; i++) JustFloat_Service();
    CHECK(tx_fifo_len == 0U, "disabled JustFloat sends no bytes");

    g_jf_enable = 1U;
    JustFloat_Service();
    CHECK(tx_fifo_len == 16U && tx_int_en == 1U && g_jf_sent_count == 0UL,
          "frame A: first 16 bytes pushed, TX int armed, not yet sent");

    JustFloat_Service();
    CHECK(tx_fifo_len == 16U && g_jf_drop_count == 1UL && g_jf_sent_count == 0UL,
          "frame while busy is dropped without blocking");

    drain_until_frame_done(TEST_FRAME_LEN);
    CHECK(g_jf_sent_count == 1UL, "sent_count incremented at final FIFO empty");

    JustFloat_Service();
    CHECK(tx_fifo_len == TEST_FRAME_LEN + 16U, "frame B accepted after frame A done");
    drain_until_frame_done(TEST_FRAME_LEN * 2U);
    CHECK(g_jf_sent_count == 2UL, "frame B complete");

    JustFloat_Service();
    drain_until_frame_done(TEST_FRAME_LEN * 3U);
    CHECK(g_jf_sent_count == 3UL, "frame C complete");
    JustFloat_TxYieldForProtocol();
    JustFloat_Service();
    CHECK(tx_fifo_len == 3U * TEST_FRAME_LEN, "yield pauses exactly one frame");
    JustFloat_Service();
    CHECK(tx_fifo_len == 3U * TEST_FRAME_LEN + 16U, "frame after yield resumes");
}

int main(void)
{
    printf("=== JustFloat Lite Waveform Tests (%uB/frame) ===\n",
           (unsigned)TEST_FRAME_LEN);
    CHECK(TEST_FRAME_LEN == 32U, "lite frame is 32 bytes");
    test_waveform_lite_modes();
    test_send_enable();
    printf("=== %s ===\n", failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED");
    return failures ? 1 : 0;
}
