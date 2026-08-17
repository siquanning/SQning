#include <math.h>
#include <stdio.h>
#include <string.h>
#include "firmware/bsp/board_config.h"
#include "firmware/control/control_closedloop.h"
#include "firmware/services/pll_host_protocol.h"
#include "firmware/services/justfloat.h"

/* 运行期调试变量（justfloat.c 不参与本测试链接，由本测试提供定义） */
volatile uint16_t g_jf_view   = DEBUG_VIEW_PLL;
volatile uint16_t g_jf_phase  = 0U;
volatile uint16_t g_jf_enable = BOARD_JUSTFLOAT_ENABLE_DEFAULT;
volatile uint16_t g_jf_lite_mode = BOARD_JUSTFLOAT_LITE_MODE_DEFAULT;

/* 协议扩展所需的桩 */
static uint16_t g_tx_frame[7];
static uint16_t g_tx_len;
void DrvSci_SendByte(uint16_t byte) { (void)byte; }
void DrvSci_SendBytes(const uint16_t *data, uint16_t len)
{
    uint16_t i;
    g_tx_len = (len <= 7U) ? len : 7U;
    for (i = 0U; i < g_tx_len; i++) g_tx_frame[i] = data[i];
}
/* 单一 TX owner 仲裁桩（justfloat.c 不参与本测试链接） */
void JustFloat_TxYieldForProtocol(void) { }
void DrvInterrupt_DisableGlobal(void) { }
void DrvInterrupt_RestoreGlobal(void) { }

static int failures;
#define CHECK(c,m) do { if (!(c)) { printf("FAIL: %s\n",m); failures++; } } while(0)
#define NEAR(a,b,e) (fabsf((a)-(b)) <= (e))

static void put_float(uint16_t frame[7], float value)
{
    union { float value; uint16_t word[2]; } bits;
    bits.value = value;
    frame[3] = bits.word[0] & 0xFFU;
    frame[4] = (bits.word[0] >> 8U) & 0xFFU;
    frame[5] = bits.word[1] & 0xFFU;
    frame[6] = (bits.word[1] >> 8U) & 0xFFU;
}

static void make_param(uint16_t frame[7], uint16_t command, float value)
{
    frame[0]=0xFFU; frame[1]=0xFFU; frame[2]=command;
    put_float(frame,value);
}

static void make_debug(uint16_t frame[7], uint16_t command, uint16_t value)
{
    frame[0]=0xFEU; frame[1]=0xFFU; frame[2]=command;
    frame[3]=0U; frame[4]=0U; frame[5]=0U; frame[6]=value;
}

static void make_get(uint16_t frame[7], uint16_t id)
{
    frame[0]=0xFCU; frame[1]=0xFFU; frame[2]=id;
    frame[3]=0U; frame[4]=0U; frame[5]=0U; frame[6]=0U;
}

static void feed(PllHostProtocol *p, const uint16_t *data, uint16_t count)
{
    uint16_t i;
    for(i=0U;i<count;i++) PllHostProtocol_ProcessByte(p,data[i]);
}

static void reset(PllHostProtocol *p)
{
    PLL_State pll;
    PLL_Init(&pll);
    g_jf_view=DEBUG_VIEW_PLL;
    g_jf_phase=0U;
    g_jf_enable=BOARD_JUSTFLOAT_ENABLE_DEFAULT;
    g_jf_lite_mode=BOARD_JUSTFLOAT_LITE_MODE_DEFAULT;
    g_tx_len = 0U;
    PllHostProtocol_Init(p);
}

static void test_float_and_atomic_commit(void)
{
    PllHostProtocol p; PLL_Params before,after; uint16_t f1[7],f2[7];
    reset(&p); PLL_ReadActiveParams(&before);
    make_param(f1,0x00U,1.0f);
    CHECK(f1[3]==0x00U && f1[4]==0x00U && f1[5]==0x80U && f1[6]==0x3FU,
          "1.0f encodes as 00 00 80 3F");
    feed(&p,f1,7U); PLL_ReadActiveParams(&after);
    CHECK(NEAR(after.kp,before.kp,1e-6f),"active Kp unchanged before 1ms commit");
    make_param(f2,0x01U,1234.0f); feed(&p,f2,7U);
    CHECK(g_pll_host_diag.valid_frames==2UL,"two contiguous frames parse");
    PllHostProtocol_CommitPending(&p); PLL_ReadActiveParams(&after);
    CHECK(NEAR(after.kp,1.0f,1e-6f)&&NEAR(after.ki,1234.0f,1e-4f),
          "1ms commit applies complete Kp/Ki snapshot together");
    CHECK(g_pll_host_diag.committed_params==1UL,"commit diagnostic increments");
}

static void test_resync_and_headers(void)
{
    PllHostProtocol p; PLL_Params a; uint16_t f[7],bad[6]={0xFFU,0x00U,1U,2U,3U,4U};
    reset(&p); make_param(f,0x00U,80.0f);
    { uint16_t garbage[3]={0x12U,0x34U,0x56U}; feed(&p,garbage,3U); }
    feed(&p,f,7U); PllHostProtocol_CommitPending(&p); PLL_ReadActiveParams(&a);
    CHECK(NEAR(a.kp,80.0f,1e-5f)&&g_pll_host_diag.resync_count>=3UL,
          "garbage prefix resynchronizes");
    feed(&p,bad,6U); feed(&p,f,7U);
    CHECK(g_pll_host_diag.invalid_frames>=1UL,"bad second header byte rejected");

    reset(&p); make_param(f,0x01U,1500.0f);
    feed(&p,f,6U); feed(&p,f,7U);
    PllHostProtocol_CommitPending(&p); PLL_ReadActiveParams(&a);
    CHECK(NEAR(a.ki,1500.0f,1e-4f)&&g_pll_host_diag.valid_frames==1UL,
          "one dropped byte recovers at following legal frame");
}

static void test_rejections(void)
{
    PllHostProtocol p; PLL_Params before,after; uint16_t f[7];
    reset(&p); PLL_ReadActiveParams(&before);
    { uint16_t unknown[7]={0xFDU,0xFFU,0U,0U,0U,0U,0U}; feed(&p,unknown,7U); }
    CHECK(g_pll_host_diag.rejected_commands>0UL,"unknown group rejected (0xFD is TX-only)");
    make_param(f,0x7FU,1.0f); feed(&p,f,7U);
    make_param(f,0x00U,sqrtf(-1.0f)); feed(&p,f,7U);
    make_param(f,0x00U,(float)HUGE_VAL); feed(&p,f,7U);
    make_param(f,0x00U,-1.0f); feed(&p,f,7U);
    make_param(f,0x01U,-1.0f); feed(&p,f,7U);
    CHECK(g_pll_host_diag.invalid_frames>=5UL,
          "unknown command NaN Inf and negative gains rejected");
    make_param(f,0x02U,51.0f); feed(&p,f,7U);
    PllHostProtocol_CommitPending(&p); PLL_ReadActiveParams(&after);
    CHECK(NEAR(after.freq_min_hz,before.freq_min_hz,1e-6f),
          "invalid min/nom/max relation preserves active snapshot");
}

static void test_debug_restore_and_error_event(void)
{
    PllHostProtocol p; PLL_Params a; SciRxQueue q; uint16_t f[7];
    reset(&p);
    make_debug(f,0x00U,0U); feed(&p,f,7U);
    CHECK(g_jf_enable==0U,"JustFloat disable command");
    make_debug(f,0x00U,1U); feed(&p,f,7U);
    CHECK(g_jf_enable==1U,"JustFloat enable command");
    {
        uint16_t view;
        for (view = DEBUG_VIEW_PLL; view <= DEBUG_VIEW_MAX; ++view) {
            make_debug(f,0x01U,view); feed(&p,f,7U);
            CHECK(g_jf_view==view,"supported debug view accepted");
        }
        make_debug(f,0x01U,DEBUG_VIEW_MAX + 1U); feed(&p,f,7U);
        CHECK(g_jf_view==DEBUG_VIEW_MAX,"unknown debug view rejected");
    }
    /* JF_PHASE: 0=自动 1..3=A/B/C，其余拒绝 */
    make_debug(f,0x03U,0U); feed(&p,f,7U); CHECK(g_jf_phase==0U,"JF_PHASE=0 auto");
    make_debug(f,0x03U,3U); feed(&p,f,7U); CHECK(g_jf_phase==3U,"JF_PHASE=3 C accepted");
    make_debug(f,0x03U,4U); feed(&p,f,7U); CHECK(g_jf_phase==3U,"JF_PHASE=4 rejected");
    /* JF_LITE_MODE: 0=AC 六路, 1=Vdc 六路, 其余拒绝 */
    make_debug(f,PLL_HOST_DEBUG_JF_LITE_MODE,JUSTFLOAT_LITE_MODE_AC);
    feed(&p,f,7U);
    CHECK(g_jf_lite_mode==JUSTFLOAT_LITE_MODE_AC,"JF_LITE_MODE=0 AC accepted");
    make_debug(f,PLL_HOST_DEBUG_JF_LITE_MODE,JUSTFLOAT_LITE_MODE_VDC);
    feed(&p,f,7U);
    CHECK(g_jf_lite_mode==JUSTFLOAT_LITE_MODE_VDC,"JF_LITE_MODE=1 Vdc accepted");
    make_debug(f,PLL_HOST_DEBUG_JF_LITE_MODE,JUSTFLOAT_LITE_MODE_VDC+1U);
    feed(&p,f,7U);
    CHECK(g_jf_lite_mode==JUSTFLOAT_LITE_MODE_VDC,"JF_LITE_MODE=2 rejected");

    make_param(f,0x00U,42.0f); feed(&p,f,7U); PllHostProtocol_CommitPending(&p);
    make_debug(f,0x02U,0U); feed(&p,f,7U); PllHostProtocol_CommitPending(&p);
    PLL_ReadActiveParams(&a);
    CHECK(NEAR(a.kp,BOARD_PLL_KP_DEFAULT,1e-6f)&&
          NEAR(a.ki,BOARD_PLL_KI_DEFAULT,1e-6f),"restore-default command commits defaults");

    SciRxQueue_Init(&q);
    SciRxQueue_PushFromIsr(&q,0xFFU,0U,1UL);
    SciRxQueue_PushFromIsr(&q,0U,1U,2UL);
    PllHostProtocol_Service(&p,&q);
    CHECK(p.count==0U&&g_pll_host_diag.resync_count>0UL,
          "SCI error event clears candidate frame");
}

static void test_runtime_params_and_get(void)
{
    PllHostProtocol p; uint16_t f[7];

    /* ---- SET 运行期变量（0x07..0x0B，范围检查） ---- */
    reset(&p);
    make_param(f,PLL_HOST_PARAM_VDC_TARGET,123.0f); feed(&p,f,7U);
    CHECK(NEAR(g_vdc_target_v,123.0f,1e-5f),"VDC_TARGET write");
    make_param(f,PLL_HOST_PARAM_VDC_TARGET,7000.0f); feed(&p,f,7U);
    CHECK(NEAR(g_vdc_target_v,123.0f,1e-5f),"VDC_TARGET out-of-range rejected");
    make_param(f,PLL_HOST_PARAM_M_LIMIT,0.5f); feed(&p,f,7U);
    CHECK(NEAR(g_m_limit,0.5f,1e-6f),"M_LIMIT write");
    make_param(f,PLL_HOST_PARAM_M_LIMIT,0.99f); feed(&p,f,7U);
    CHECK(NEAR(g_m_limit,0.5f,1e-6f),"M_LIMIT >0.98 rejected");
    make_param(f,PLL_HOST_PARAM_IAMP_LIMIT,2.0f); feed(&p,f,7U);
    CHECK(NEAR(g_i_limit_a,2.0f,1e-5f),"IAMP_LIMIT write");
    make_param(f,PLL_HOST_PARAM_CURRENT_KP,8.0f); feed(&p,f,7U);
    CHECK(NEAR(g_kp_i,8.0f,1e-5f),"CURRENT_KP write");
    make_param(f,PLL_HOST_PARAM_CURRENT_KI,2000.0f); feed(&p,f,7U);
    CHECK(NEAR(g_ki_i,2000.0f,1e-4f),"CURRENT_KI write");
    make_param(f,PLL_HOST_PARAM_CURRENT_KI,(float)HUGE_VAL); feed(&p,f,7U);
    CHECK(NEAR(g_ki_i,2000.0f,1e-4f),"CURRENT_KI non-finite rejected");

    /* ---- GET_PARAM（0xFC 请求 → 0xFD 响应帧） ---- */
    reset(&p);
    make_get(f,JF_PARAM_ID_VDC_TARGET); feed(&p,f,7U);
    CHECK(g_tx_len==7U && g_tx_frame[0]==0xFDU && g_tx_frame[1]==0xFFU &&
          g_tx_frame[2]==JF_PARAM_ID_VDC_TARGET,
          "GET emits 0xFD response with echoed id");
    {
        union { float value; uint16_t word[2]; } bits;
        bits.word[0] = g_tx_frame[3] | (g_tx_frame[4] << 8U);
        bits.word[1] = g_tx_frame[5] | (g_tx_frame[6] << 8U);
        CHECK(NEAR(bits.value,g_vdc_target_v,1e-5f),
              "GET response carries current value");
    }

    make_get(f,JF_PARAM_ID_M_LIMIT); feed(&p,f,7U);
    {
        union { float value; uint16_t word[2]; } bits;
        bits.word[0] = g_tx_frame[3] | (g_tx_frame[4] << 8U);
        bits.word[1] = g_tx_frame[5] | (g_tx_frame[6] << 8U);
        CHECK(g_tx_frame[2]==JF_PARAM_ID_M_LIMIT &&
              NEAR(bits.value,g_m_limit,1e-6f),"GET M_LIMIT round-trip");
    }

    make_get(f,JF_PARAM_ID_JF_VIEW); feed(&p,f,7U);
    CHECK(g_tx_frame[2]==JF_PARAM_ID_JF_VIEW &&
          g_tx_frame[3]==DEBUG_VIEW_PLL && g_tx_frame[4]==0U,
          "GET JF_VIEW round-trip (view 0)");

    g_jf_lite_mode = JUSTFLOAT_LITE_MODE_VDC;
    make_get(f,JF_PARAM_ID_JF_LITE_MODE); feed(&p,f,7U);
    CHECK(g_tx_frame[2]==JF_PARAM_ID_JF_LITE_MODE &&
          g_tx_frame[3]==0U && g_tx_frame[4]==0U &&
          g_tx_frame[5]==0x80U && g_tx_frame[6]==0x3FU,
          "GET JF_LITE_MODE round-trip (mode 1)");

    make_get(f,0x7FU); feed(&p,f,7U);
    CHECK(g_pll_host_diag.rejected_commands>0UL,
          "GET unknown id rejected without response");
}

int main(void)
{
    printf("=== PLL Host Protocol Tests ===\n");
    test_float_and_atomic_commit();
    test_resync_and_headers();
    test_rejections();
    test_debug_restore_and_error_event();
    test_runtime_params_and_get();
    printf("=== %s ===\n",failures?"SOME TESTS FAILED":"ALL TESTS PASSED");
    return failures?1:0;
}
