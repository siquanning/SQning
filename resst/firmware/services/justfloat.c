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
volatile uint16_t g_jf_phase  = 0U;
volatile uint16_t g_jf_enable = BOARD_JUSTFLOAT_ENABLE_DEFAULT;
volatile uint16_t g_jf_lite_mode = BOARD_JUSTFLOAT_LITE_MODE_DEFAULT;

/* JustFloat TX 统计（见 justfloat.h）：完整发送帧数 / busy 丢弃帧数。 */
volatile uint32_t g_jf_sent_count = 0UL;
volatile uint32_t g_jf_drop_count  = 0UL;

#if BOARD_DEBUG_JUSTFLOAT_ENABLE

/*
 * 帧缓冲: 7ch × 4B + 4B 帧尾 = 32 字节。
 * C28x 的 char 是 16 位，stdint.h 无 uint8_t — 用 uint16_t 数组，
 * 每个元素只存一个字节值 (0~255)，写入 SCI TX FIFO 时 & 0xFF 截低 8 位。
 */
#define JUSTFLOAT_FRAME_LEN   (JUSTFLOAT_CH_COUNT * 4U + 4U)

static uint16_t s_frame[JUSTFLOAT_FRAME_LEN];

/*
 * 非阻塞 TX 状态（SCI-C TX FIFO 中断，PIE 8.6）。
 * 1ms 任务只组帧 + 提交首批 ≤16 字节；FIFO 发空触发 TX 中断，
 * ISR 继续搬运剩余字节（16→16），整帧最后一字节物理发出后
 * （FIFO empty 中断）清 busy 并禁止 TXFFIENA，防止空中断风暴。
 * s_tx_busy/s_tx_paused 均为单字原子读写，前台与 ISR 无撕裂。
 */
static uint16_t s_tx_buf[JUSTFLOAT_FRAME_LEN];  /* 提交帧的独立拷贝（ISR 后台搬运期间不被覆盖） */
static uint16_t s_tx_busy   = 0U;               /* 1 = 有帧发送中（TXFFIENA 已使能） */
static uint16_t s_tx_pos    = 0U;               /* 已写入 TX FIFO 的字节数 */
static uint16_t s_tx_len    = 0U;               /* 当前帧总长 */
static uint16_t s_tx_paused = 0U;               /* 1 = 协议独占发送，下一帧让路（只暂停一次） */

/*
 * JustFloat_GetChannels — 从统一 DebugSnapshot 读取一帧通道数据。
 * 快照在 20kHz 控制 ISR 按 1kHz 更新；此处 DINT 保护下整帧拷贝，
 * 保证各通道来自同一个控制时刻。
 * 仅读取观测数据，绝不触发任何控制/安全行为。
 * g_jf_lite_mode：0=线电压+电流，1=Vdc，2=相电压+PLL跟随，3=Vdc+Iac/vd_ctrl/iamp；非法 mode 回退 0。
 */
void JustFloat_GetChannels(float *ch)
{
    DebugSnapshot s;

    if (ch == ((float *)0)) return;

    DrvInterrupt_DisableGlobal();
    s = g_dbg_snap;
    DrvInterrupt_RestoreGlobal();

    if (g_jf_lite_mode == JUSTFLOAT_LITE_MODE_VDC) {
        ch[0] = s.vdc[0];
        ch[1] = s.vdc[1];
        ch[2] = s.vdc[2];
        ch[3] = s.vdc[3];
        ch[4] = s.vdc[4];
        ch[5] = s.vdc[5];
        ch[6] = 0.0f;
        return;
    }
    if (g_jf_lite_mode == JUSTFLOAT_LITE_MODE_PLL) {
        ch[0] = s.vac[0];
        ch[1] = s.vac[1];
        ch[2] = s.vac[2];
        ch[3] = s.pll_vmag * cosf(s.pll_theta);
        ch[4] = s.pll_vmag * cosf(s.pll_theta - JUSTFLOAT_TWO_PI_3);
        ch[5] = s.pll_vmag * cosf(s.pll_theta + JUSTFLOAT_TWO_PI_3);
        ch[6] = 0.0f;
        return;
    }
    if (g_jf_lite_mode == JUSTFLOAT_LITE_MODE_DQ) {
        ch[0] = s.vdc_avg;
        ch[1] = s.iac_obs;
        ch[2] = s.vd_ctrl;
        ch[3] = s.iamp;
        ch[4] = s.id;
        ch[5] = s.iq;
        ch[6] = s.m;
        return;
    }
    ch[0] = s.vline[0];
    ch[1] = s.vline[1];
    ch[2] = s.vline[2];
    ch[3] = s.iac[0];
    ch[4] = s.iac[1];
    ch[5] = s.iac[2];
    ch[6] = 0.0f;
}

void JustFloat_Send(const float *ch, uint16_t ch_count)
{
    uint16_t len = 0U;
    uint16_t n;
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

    /*
     * 非阻塞提交（单一 TX owner）:
     *  - 上一帧未发完（busy=1）→ 直接丢弃，绝不等待/阻塞控制路径。
     *  - 空闲 → 拷贝到独立 TX 缓冲（ISR 后台搬运期间不被下一帧覆盖），
     *    一次尽量填满 16 字节 FIFO；有剩余字节才使能 TX FIFO 中断，
     *    由 ISR 在 FIFO 空时续搬。g_jf_sent_count 由最终 FIFO empty ISR
     *    在整帧发送完成后递增（本函数不递增）。
     */
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

    n = (len > 16U) ? 16U : len;
    for (i = 0U; i < n; ++i) {
        DrvSci_TxPutByte(s_tx_buf[s_tx_pos++]);
    }
    /*
     * 无条件使能 TX FIFO 中断：提交时 FIFO 必空（busy=0 表示上一帧已
     * 完整发完），填字节后 FIFO 非空不会立即触发；FIFO 发空后由 ISR
     * 续搬剩余字节并在最后一字节发出后清 busy（对 len≤16 的短帧同样
     * 适用，避免 busy 永不清除）。
     */
    DrvSci_ClearTxIntFlag();   /* 清残留标志后使能，避免提交瞬间误触发 */
    DrvSci_TxIntEnable(1U);
}

/*
 * 1ms tick — 当前每次调用发一帧，输出周期1ms（1kHz）。
 * 目标576000波特（实际由 board_clock_profile.h 的 LSPCLK 派生：
 * TARGET20→≈568182, DEV30→≈576923）下 32 字节帧在线时间约 0.56ms；
 * 调用点位于1ms安全任务末尾，不延迟本拍故障封锁。
 * 20kHz 控制 ISR 内不做任何 SCI 发送。
 */
void JustFloat_Service(void)
{
    static uint16_t s_div = 0U;

    if (g_jf_enable == 0U) {
        s_div = 0U;
        return;
    }

    /*
     * 协议独占发送让路：JustFloat_TxYieldForProtocol() 已暂停本帧。
     * 只跳过一帧（清标志后下一周期恢复），给协议 response 尾部留出
     * ≥1ms 物理发送窗口，防止半帧交错。
     */
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
        JustFloat_GetChannels(ch);
        JustFloat_Send(ch, JUSTFLOAT_CH_COUNT);
    }
}

/*
 * SCI-C TX FIFO 空中断（PIE 8.6）— 非阻塞发送的核心搬运者。
 *
 * 进入本中断时 TX FIFO 必为空（TXFFIL=0：TXFFST≤0 才置 TXFFINT），
 * 因此：
 *   - 缓冲仍有剩余 → 一次尽量填满 16 字节，保持 TXFFIENA 使能，
 *     等 FIFO 再次发空触发下一次中断（32B 帧 = 16+16 两批）。
 *   - 缓冲已耗尽 → 说明整帧最后一字节已物理发出（FIFO 空），
 *     帧完成：清 busy、禁止 TXFFIENA（防止 FIFO 恒空中断风暴）、
 *     g_jf_sent_count++。
 * 中断内不做任何等待，也不访问控制/安全状态。
 */
#ifdef __TMS320C28XX__
__interrupt void JustFloat_ScicTxIsr(void)
#else
void JustFloat_ScicTxIsr(void)
#endif
{
    uint16_t n;
    uint16_t i;

    if (s_tx_pos < s_tx_len) {
        n = (uint16_t)(s_tx_len - s_tx_pos);
        if (n > 16U) n = 16U;
        for (i = 0U; i < n; ++i) {
            DrvSci_TxPutByte(s_tx_buf[s_tx_pos++]);
        }
    } else {
        s_tx_busy = 0U;
        DrvSci_TxIntEnable(0U);
        g_jf_sent_count++;
    }
    DrvSci_ClearTxIntFlag();
    DrvInterrupt_AckGroup8();
}

/*
 * 单一 TX owner 仲裁 — 协议层独占发送前调用（见 justfloat.h 注释）。
 * 等待期间 JustFloat 不会提交新帧（主循环同一线程串行），busy 只可能
 * 被 TX ISR 清零，因此等待必然有界（最长一帧 ≈0.63ms@576000bps）。
 */
void JustFloat_TxYieldForProtocol(void)
{
    while (s_tx_busy != 0U) {
        /* 有界等待当前 JustFloat 帧完整发送结束（TX ISR 后台搬运） */
    }
    s_tx_paused = 1U;   /* 下一帧让路，由 JustFloat_Service 消费 */
}

#endif /* BOARD_DEBUG_JUSTFLOAT_ENABLE */
