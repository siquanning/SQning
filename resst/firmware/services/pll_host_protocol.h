/* Created by Siquanning */
#ifndef PLL_HOST_PROTOCOL_H
#define PLL_HOST_PROTOCOL_H

#include <stdint.h>
#include "firmware/app/sci_rx_queue.h"
#include "firmware/control/control_pll.h"

#define PLL_HOST_FRAME_LEN       7U
#define PLL_HOST_GROUP_PARAMS    0xFFU
#define PLL_HOST_GROUP_DEBUG     0xFEU
#define PLL_HOST_GROUP_GET       0xFCU   /* RX: 读参数请求 */
#define PLL_HOST_GROUP_RESPONSE  0xFDU   /* TX: 参数读取响应 */
#define PLL_HOST_MARKER          0xFFU

/* ---- PARAMS 组命令 (SET, float 值) ----
 * 0x00..0x06: PLL 参数（原有，暂存后 1ms 统一提交）
 * 0x07..0x0B: 运行期变量（新增，合法范围内直接写入 g_xxx） */
#define PLL_HOST_PARAM_VDC_TARGET   0x07U   /* g_vdc_target_v */
#define PLL_HOST_PARAM_IAMP_LIMIT   0x08U   /* g_i_limit_a */
#define PLL_HOST_PARAM_M_LIMIT      0x09U   /* g_m_limit */
#define PLL_HOST_PARAM_CURRENT_KP   0x0AU   /* g_kp_i */
#define PLL_HOST_PARAM_CURRENT_KI   0x0BU   /* g_ki_i */

/* ---- DEBUG 组命令 (uint16 值) ----
 * 0x00 = JF_ENABLE (0/1), 0x01 = JF_VIEW (0..DEBUG_VIEW_MAX), 0x02 = 恢复默认PLL
 * 0x03 = JF_PHASE (0=自动跟随, 1..3=A/B/C)
 * 0x04 = JF_LITE_MODE (0=Vac/Iac, 1=Vdc1..Vdc6, 2=当前相双闭环) */
#define PLL_HOST_DEBUG_JF_PHASE     0x03U
#define PLL_HOST_DEBUG_JF_LITE_MODE 0x04U

/* ---- GET_PARAM 参数 ID 白名单 (group 0xFC 使用, flat 编号) ----
 * 只允许读取以下白名单参数，禁止任意 RAM 地址访问。 */
#define JF_PARAM_ID_PLL_KP          0x00U
#define JF_PARAM_ID_PLL_KI          0x01U
#define JF_PARAM_ID_VDC_TARGET      0x02U
#define JF_PARAM_ID_IAMP_LIMIT      0x03U
#define JF_PARAM_ID_M_LIMIT         0x04U
#define JF_PARAM_ID_CURRENT_KP      0x05U
#define JF_PARAM_ID_CURRENT_KI      0x06U
#define JF_PARAM_ID_JF_ENABLE       0x07U
#define JF_PARAM_ID_JF_VIEW         0x08U
#define JF_PARAM_ID_JF_PHASE        0x09U
#define JF_PARAM_ID_JF_LITE_MODE    0x0AU
#define JF_PARAM_ID_MAX             0x0AU

typedef struct {
    uint32_t rx_bytes;
    uint32_t valid_frames;
    uint32_t invalid_frames;
    uint32_t rejected_commands;
    uint32_t committed_params;
    uint32_t resync_count;
} PllHostProtocolDiag;

typedef struct {
    uint16_t frame[PLL_HOST_FRAME_LEN];
    uint16_t count;
    uint16_t pending_dirty;
    PLL_Params pending;
} PllHostProtocol;

extern volatile PllHostProtocolDiag g_pll_host_diag;

void PllHostProtocol_Init(PllHostProtocol *protocol);
void PllHostProtocol_ProcessByte(PllHostProtocol *protocol, uint16_t data);
void PllHostProtocol_OnRxError(PllHostProtocol *protocol);
void PllHostProtocol_Service(PllHostProtocol *protocol, SciRxQueue *queue);
void PllHostProtocol_CommitPending(PllHostProtocol *protocol);

#endif
