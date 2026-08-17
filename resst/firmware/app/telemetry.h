/* Created by Siquanning */
#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 快速快照: ISR 写入, 固定开销 ---- */
typedef struct
{
    uint16_t version;
    uint16_t state;
    uint16_t adc_raw[2];
    uint16_t cmpa[3];
    uint16_t cmpb[3];
    uint16_t output_valid;
    uint16_t trip_flags;
    uint16_t fault_code;
    uint16_t step_count;
    uint16_t reserved[1];
} TelemetryFastSnapshot;

/* ---- 遥测: 双缓冲, ISR 安全 ---- */
typedef struct
{
    TelemetryFastSnapshot buffer[2];
    volatile uint16_t     active_idx;
    uint16_t              read_idx;
    uint32_t              overrun_count;
    uint32_t              write_count;
} Telemetry;

/* ---- API ---- */

void Telemetry_Init(Telemetry *t);

/*
 * ISR 快速写入: 从当前控制状态填充快照
 * 固定开销 — 除版本递增外无分支
 */
void Telemetry_WriteFastSnapshot(Telemetry *t,
                                 uint16_t state,
                                 const uint16_t adc_raw[2],
                                 const uint16_t cmpa[3],
                                 const uint16_t cmpb[3],
                                 uint16_t output_valid,
                                 uint16_t trip_flags,
                                 uint16_t fault_code,
                                 uint16_t step_count);

/*
 * 后台一致性读取: 交换 active buffer, 读取稳定副本
 * 从 100ms 调度器任务调用
 * 返回 1 表示快照一致, 0 表示检测到覆盖
 */
int Telemetry_ReadSnapshot(Telemetry *t,
                           TelemetryFastSnapshot *out);

#ifdef __cplusplus
}
#endif

#endif /* TELEMETRY_H */
