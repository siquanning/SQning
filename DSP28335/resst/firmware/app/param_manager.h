#ifndef PARAM_MANAGER_H
#define PARAM_MANAGER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 控制参数 (16 字, 2 的幂) ---- */
typedef struct
{
    uint16_t version;
    int16_t  m_permill[3];              /* m ∈ [-980, +980] per-mill, 三通道独立 */
    uint16_t control_mode;
    uint16_t tbprd;
    uint16_t adc_safe_min;
    uint16_t adc_safe_max;
    uint16_t fault_thresh_adc_stuck;
    uint16_t fault_thresh_sched_miss;
    uint16_t fault_thresh_spi_timeout;
    uint16_t reserved[5];
} ControlParams;

/* ---- 拒绝原因码 ---- */
#define PARAM_REJECT_NONE             0U
#define PARAM_REJECT_VERSION          1U
#define PARAM_REJECT_DUTY_RANGE       2U
#define PARAM_REJECT_CONTROL_MODE     3U
#define PARAM_REJECT_TBPRD_RANGE      4U
#define PARAM_REJECT_ADC_RANGE_ORDER  5U
#define PARAM_REJECT_ADC_MAX_EXCEED   6U
#define PARAM_REJECT_THRESH_ZERO      7U
#define PARAM_REJECT_INDUSTRIAL_CAP   8U
#define PARAM_REJECT_M_RANGE          9U

/* ---- 参数管理器 ---- */
typedef struct
{
    ControlParams pending;
    ControlParams active;
    uint32_t      commit_requested;
    uint32_t      commit_count;
    uint32_t      reject_count;
    uint16_t      last_reject_reason;
    uint16_t      reserved_pad;
} ParamManager;

/* ---- API ---- */

/*
 * 初始化为安全默认值, 启动时调用一次
 * m_permill[3] 默认 0 (50% 占空比, 零调制点)
 */
void Param_Init(ParamManager *pm, uint16_t tbprd);

/*
 * 从外部源填充 pending (调度器/主机测试/未来通信)
 * 仅写 .pending — 绝不触碰 .active
 */
void Param_SubmitPending(ParamManager *pm,
                         const ControlParams *new_params);

/*
 * 请求在下一个 1ms 边界提交
 * 后台设置此标志; 1ms 任务执行原子复制
 */
void Param_RequestCommit(ParamManager *pm);

/*
 * 验证 pending 参数, 有效则提交 pending → active
 * 1ms 调度器任务的唯一入口
 * 内部检查 commit_requested, 验证, 提交, 更新诊断
 *
 * 返回:
 *   PARAM_COMMIT_OK        (0) — 无待处理, 或提交成功
 *   PARAM_COMMIT_REJECTED  (1) — 验证失败, 已记录拒绝
 */
#define PARAM_COMMIT_OK       0
#define PARAM_COMMIT_REJECTED  1

int Param_ServicePendingCommit(ParamManager *pm);

/*
 * 读取诊断快照: commit 次数, reject 次数, 最后拒绝原因
 */
void Param_GetDiagSnapshot(const ParamManager *pm,
                           uint32_t *commit_count_out,
                           uint32_t *reject_count_out,
                           uint16_t *last_reject_reason_out);

/*
 * 验证并提交 pending → active (内部 API)
 * 返回值: 1=已提交, 0=已拒绝 (检查 last_reject_reason)
 */
int Param_CheckPendingCommit(ParamManager *pm);

/*
 * 读取 active 参数到调用者持有的结构体, ISR 安全
 */
void Param_ReadActive(const ParamManager *pm,
                      ControlParams *out);

/*
 * 纯函数: 验证 ControlParams 的范围和 profile 规则
 * 返回 PARAM_REJECT_NONE (0) 表示有效, 否则返回 PARAM_REJECT_* 码
 */
uint16_t Param_Validate(const ControlParams *params,
                        const ControlParams *current_active);

#ifdef __cplusplus
}
#endif

#endif /* PARAM_MANAGER_H */
