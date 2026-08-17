/* Created by Siquanning */
#include "firmware/services/pll_host_protocol.h"
#include "firmware/bsp/board_config.h"
#include "firmware/services/justfloat.h"
#include "firmware/control/control_closedloop.h"
#include "firmware/drivers/drv_sci.h"
#include "firmware/drivers/drv_interrupt.h"

volatile PllHostProtocolDiag g_pll_host_diag;

static uint16_t is_group(uint16_t value)
{
    return ((value == PLL_HOST_GROUP_PARAMS) ||
            (value == PLL_HOST_GROUP_DEBUG) ||
            (value == PLL_HOST_GROUP_GET)) ? 1U : 0U;
}

static uint16_t finite_local(float x)
{
    return ((x == x) && (x <= 3.402823466e+38F) &&
            (x >= -3.402823466e+38F)) ? 1U : 0U;
}

static float decode_float_le(const uint16_t *data)
{
    union {
        float value;
        uint16_t word[2];
    } bits;
    bits.word[0] = (data[0] & 0x00FFU) | ((data[1] & 0x00FFU) << 8U);
    bits.word[1] = (data[2] & 0x00FFU) | ((data[3] & 0x00FFU) << 8U);
    return bits.value;
}

static uint16_t value_in_field_range(uint16_t command, float value)
{
    if (finite_local(value) == 0U) return 0U;
    switch (command) {
    case 0x00U: return (value >= BOARD_PLL_KP_MIN && value <= BOARD_PLL_KP_MAX);
    case 0x01U: return (value >= BOARD_PLL_KI_MIN && value <= BOARD_PLL_KI_MAX);
    case 0x02U:
    case 0x03U:
    case 0x04U:
        return (value >= BOARD_PLL_FREQ_MIN_ALLOWED_HZ &&
                value <= BOARD_PLL_FREQ_MAX_ALLOWED_HZ);
    case 0x05U:
    case 0x06U:
        return (value >= BOARD_PLL_VQ_RATIO_MIN &&
                value <= BOARD_PLL_VQ_RATIO_MAX);
    /* 运行期变量（直接写入 g_xxx，上下限见 board_config.h） */
    case PLL_HOST_PARAM_VDC_TARGET:
        return (value >= BOARD_VDC_TARGET_MIN_V && value <= BOARD_VDC_TARGET_MAX_V);
    case PLL_HOST_PARAM_IAMP_LIMIT:
        return (value >= BOARD_I_LIMIT_MIN_A && value <= BOARD_I_LIMIT_MAX_A);
    case PLL_HOST_PARAM_M_LIMIT:
        return (value >= BOARD_M_LIMIT_MIN && value <= BOARD_M_LIMIT_MAX);
    case PLL_HOST_PARAM_CURRENT_KP:
        return (value >= BOARD_CURRENT_KP_MIN && value <= BOARD_CURRENT_KP_MAX);
    case PLL_HOST_PARAM_CURRENT_KI:
        return (value >= BOARD_CURRENT_KI_MIN && value <= BOARD_CURRENT_KI_MAX);
    default: return 0U;
    }
}

/*
 * 运行期变量直接写入（0x07..0x0B）。
 * 32 位 float 写对 C28x 非原子，DINT 保护避免 20kHz ISR 读到撕裂值。
 * 只修改对应 g_xxx 运行期变量，不触碰任何算法/安全链。
 */
static uint16_t apply_runtime_param(uint16_t command, float value)
{
    if (value_in_field_range(command, value) == 0U) return 0U;
    DrvInterrupt_DisableGlobal();
    switch (command) {
    case PLL_HOST_PARAM_VDC_TARGET: g_vdc_target_v = value; break;
    case PLL_HOST_PARAM_IAMP_LIMIT: g_i_limit_a = value; break;
    case PLL_HOST_PARAM_M_LIMIT:    g_m_limit = value; break;
    case PLL_HOST_PARAM_CURRENT_KP: g_kp_i = value; break;
    case PLL_HOST_PARAM_CURRENT_KI: g_ki_i = value; break;
    default:
        DrvInterrupt_RestoreGlobal();
        return 0U;
    }
    DrvInterrupt_RestoreGlobal();
    return 1U;
}

static uint16_t stage_param(PllHostProtocol *p, uint16_t command, float value)
{
    if (value_in_field_range(command, value) == 0U) return 0U;
    switch (command) {
    case 0x00U: p->pending.kp = value; break;
    case 0x01U: p->pending.ki = value; break;
    case 0x02U: p->pending.freq_min_hz = value; break;
    case 0x03U: p->pending.freq_max_hz = value; break;
    case 0x04U: p->pending.freq_nom_hz = value; break;
    case 0x05U: p->pending.vq_lock_ratio = value; break;
    case 0x06U: p->pending.vq_unlock_ratio = value; break;
    default: return 0U;
    }
    p->pending_dirty = 1U;
    return 1U;
}

static uint16_t apply_debug(PllHostProtocol *p, uint16_t command,
                            uint16_t value)
{
    switch (command) {
    case 0x00U:
        if (value > 1U) return 0U;
        g_jf_enable = value;
        return 1U;
    case 0x01U:
        if (value > DEBUG_VIEW_MAX) return 0U;
        g_jf_view = value;
        return 1U;
    case 0x02U:
        PLL_GetDefaultParams(&p->pending);
        p->pending_dirty = 1U;
        return 1U;
    case PLL_HOST_DEBUG_JF_PHASE:
        if (value > CTRL_TEST_PHASE_C) return 0U;   /* 0=自动, 1..3=A/B/C */
        g_jf_phase = value;
        return 1U;
    case PLL_HOST_DEBUG_JF_LITE_MODE:
        if (value > JUSTFLOAT_LITE_MODE_VDC) return 0U;
        g_jf_lite_mode = value;
        return 1U;
    default:
        return 0U;
    }
}

/*
 * GET_PARAM 响应帧 (TX): [0xFD][0xFF][param_id][float LE 4B] = 7 字节。
 * 与 JustFloat 帧 (36B, 帧尾 00 00 80 7F) 都从前台发送；
 * GET 响应为一次性短帧，PC 端按 0xFD 帧头区分（会打断一帧 JustFloat，属预期）。
 */
static void send_response(uint16_t id, float value)
{
    uint16_t frame[PLL_HOST_FRAME_LEN];
    union {
        float value;
        uint16_t word[2];
    } bits;

    bits.value = value;
    frame[0] = PLL_HOST_GROUP_RESPONSE;
    frame[1] = PLL_HOST_MARKER;
    frame[2] = id;
    frame[3] = bits.word[0] & 0x00FFU;
    frame[4] = (bits.word[0] >> 8U) & 0x00FFU;
    frame[5] = bits.word[1] & 0x00FFU;
    frame[6] = (bits.word[1] >> 8U) & 0x00FFU;

#if BOARD_DEBUG_JUSTFLOAT_ENABLE
    /*
     * 单一 TX owner 仲裁：等当前 JustFloat 帧完整发送结束并暂停下一帧，
     * 保证本 response 独占 SCI-C TX 发送，不被 JustFloat 半帧打断。
     */
    JustFloat_TxYieldForProtocol();
#endif
    DrvSci_SendBytes(frame, PLL_HOST_FRAME_LEN);
}

/*
 * GET_PARAM — 只读白名单参数（见 pll_host_protocol.h JF_PARAM_ID_*）。
 * 禁止任意 RAM 地址读取；未知 ID 拒绝且不回响应。
 * 前台上下文调用（PllHostProtocol_Service），阻塞发送可接受。
 */
static uint16_t handle_get(uint16_t id)
{
    PLL_Params params;
    float value = 0.0f;
    uint16_t ok = 1U;

    switch (id) {
    case JF_PARAM_ID_PLL_KP:       PLL_ReadActiveParams(&params); value = params.kp; break;
    case JF_PARAM_ID_PLL_KI:       PLL_ReadActiveParams(&params); value = params.ki; break;
    case JF_PARAM_ID_VDC_TARGET:   value = g_vdc_target_v; break;
    case JF_PARAM_ID_IAMP_LIMIT:   value = g_i_limit_a; break;
    case JF_PARAM_ID_M_LIMIT:      value = g_m_limit; break;
    case JF_PARAM_ID_CURRENT_KP:   value = g_kp_i; break;
    case JF_PARAM_ID_CURRENT_KI:   value = g_ki_i; break;
    case JF_PARAM_ID_JF_ENABLE:    value = (float)g_jf_enable; break;
    case JF_PARAM_ID_JF_VIEW:      value = (float)g_jf_view; break;
    case JF_PARAM_ID_JF_PHASE:     value = (float)g_jf_phase; break;
    case JF_PARAM_ID_JF_LITE_MODE: value = (float)g_jf_lite_mode; break;
    default: ok = 0U; break;
    }

    if (ok != 0U) {
        send_response(id, value);
    }
    return ok;
}

static uint16_t parse_frame(PllHostProtocol *p)
{
    uint16_t group = p->frame[0];
    uint16_t command = p->frame[2];
    if ((is_group(group) == 0U) || (p->frame[1] != PLL_HOST_MARKER))
        return 0U;
    if (group == PLL_HOST_GROUP_PARAMS) {
        if (command >= PLL_HOST_PARAM_VDC_TARGET)
            return apply_runtime_param(command, decode_float_le(&p->frame[3]));
        return stage_param(p, command, decode_float_le(&p->frame[3]));
    }
    if (group == PLL_HOST_GROUP_GET)
        return handle_get(command);
    return apply_debug(p, command, p->frame[6] & 0x00FFU);
}

static void retain_candidate_suffix(PllHostProtocol *p)
{
    uint16_t start, i, retained = 0U;
    for (start = 1U; start < PLL_HOST_FRAME_LEN; start++) {
        if (is_group(p->frame[start]) != 0U) {
            if ((start + 1U >= PLL_HOST_FRAME_LEN) ||
                (p->frame[start + 1U] == PLL_HOST_MARKER)) {
                retained = PLL_HOST_FRAME_LEN - start;
                for (i = 0U; i < retained; i++) p->frame[i] = p->frame[start+i];
                break;
            }
        }
    }
    p->count = retained;
}

void PllHostProtocol_Init(PllHostProtocol *p)
{
    uint16_t i;
    if (p == ((PllHostProtocol *)0)) return;
    for (i = 0U; i < PLL_HOST_FRAME_LEN; i++) p->frame[i] = 0U;
    p->count = 0U;
    p->pending_dirty = 0U;
    PLL_ReadActiveParams(&p->pending);
    g_pll_host_diag.rx_bytes = 0UL;
    g_pll_host_diag.valid_frames = 0UL;
    g_pll_host_diag.invalid_frames = 0UL;
    g_pll_host_diag.rejected_commands = 0UL;
    g_pll_host_diag.committed_params = 0UL;
    g_pll_host_diag.resync_count = 0UL;
}

void PllHostProtocol_ProcessByte(PllHostProtocol *p, uint16_t data)
{
    uint16_t byte = data & 0x00FFU;
    if (p == ((PllHostProtocol *)0)) return;
    g_pll_host_diag.rx_bytes++;
    if (p->count == 0U) {
        if (is_group(byte) != 0U) { p->frame[0] = byte; p->count = 1U; }
        else { g_pll_host_diag.rejected_commands++; g_pll_host_diag.resync_count++; }
        return;
    }
    if (p->count == 1U && byte != PLL_HOST_MARKER) {
        g_pll_host_diag.invalid_frames++;
        g_pll_host_diag.resync_count++;
        if (is_group(byte) != 0U) { p->frame[0] = byte; p->count = 1U; }
        else p->count = 0U;
        return;
    }
    p->frame[p->count++] = byte;
    if (p->count == PLL_HOST_FRAME_LEN) {
        if (parse_frame(p) != 0U) {
            g_pll_host_diag.valid_frames++;
            p->count = 0U;
        } else {
            g_pll_host_diag.invalid_frames++;
            g_pll_host_diag.rejected_commands++;
            g_pll_host_diag.resync_count++;
            retain_candidate_suffix(p);
        }
    }
}

void PllHostProtocol_OnRxError(PllHostProtocol *p)
{
    if (p == ((PllHostProtocol *)0)) return;
    p->count = 0U;
    g_pll_host_diag.invalid_frames++;
    g_pll_host_diag.resync_count++;
}

void PllHostProtocol_Service(PllHostProtocol *p, SciRxQueue *queue)
{
    SciRxItem item;
    if ((p == ((PllHostProtocol *)0)) || (queue == ((SciRxQueue *)0))) return;
    while (SciRxQueue_Pop(queue, &item)) {
        if (item.error_flags != 0U) PllHostProtocol_OnRxError(p);
        else PllHostProtocol_ProcessByte(p, item.data);
    }
}

void PllHostProtocol_CommitPending(PllHostProtocol *p)
{
    if ((p == ((PllHostProtocol *)0)) || (p->pending_dirty == 0U)) return;
    if (PLL_ValidateParams(&p->pending) != 0U) {
        PLL_CommitParams(&p->pending);
        g_pll_host_diag.committed_params++;
    } else {
        PLL_ReadActiveParams(&p->pending);
        g_pll_host_diag.rejected_commands++;
    }
    p->pending_dirty = 0U;
}
