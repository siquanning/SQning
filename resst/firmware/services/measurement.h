#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#include <stdint.h>

/*
 * MeasurementSample — 瞬时物理量快照，所有值为瞬时值 (非 RMS)。
 *
 * 由 Measurement_Update() 从 g_vdc_raw/g_vac_raw/g_iac_raw 换算出。
 * 所有换算参数均来自 board_config.h 的硬件链宏推导，.c 中无手填增益。
 */
typedef struct {
    float vdc_v[6];   /* Vdc1..Vdc6  瞬时直流母线电压 [V] */
    float vac_v[3];   /* Va, Vb, Vc   瞬时交流端电压   [V] */
    float iac_a[3];   /* Ia, Ib, Ic   瞬时交流电流     [A] */
} MeasurementSample;

/*
 * 全局测量快照 — 1 kHz 由 App_Service1ms 刷新，CCS debugger 可直接 watch。
 */
extern MeasurementSample g_measurement;

/*
 * 可复用的单通道换算函数 — 纯函数、无状态。
 * 后续 20 kHz 快速控制路径可直接调用，无需经过 Measurement_Update()。
 */

/* Vdc: V_primary = raw × VREF/MAX_COUNT × CT2_PRI/CT2_SEC × CT1_PRI/CT1_SEC / GAIN */
float Measurement_ConvertVdc(uint16_t raw);

/*
 * Vac: delta = (int32_t)raw - offset
 *      V_primary = polarity × delta × VREF/MAX_COUNT / (TIA×GAIN)
 *                × CT2_PRI_V/CT2_SEC_A × CT1_PRI_V/CT1_SEC_V
 */
float Measurement_ConvertVac(uint16_t raw, uint16_t offset, float polarity);

/*
 * Iac: delta = (int32_t)raw - offset
 *      I_primary = polarity × delta × VREF/MAX_COUNT / (TIA×GAIN)
 *                × CT2_PRI_A/CT2_SEC_A × CT1_PRI_A/CT1_SEC_A
 */
float Measurement_ConvertIac(uint16_t raw, uint16_t offset, float polarity);

/*
 * Measurement_Update — 1 kHz 调试路径。
 * 从 ADC ISR 写入的 g_vdc_raw/g_vac_raw/g_iac_raw 读取原始值，
 * 填入 g_measurement，每 1ms 刷新一次。
 *
 * 不修改任何 raw 全局量，不影响 Modbus HR0~11。
 */
void Measurement_Update(MeasurementSample *out);

#endif
