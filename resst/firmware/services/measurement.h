/* Created by Siquanning */
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
 * 每个采样通道独立的ADC零偏运行变量，单位均为ADC count。
 * 可在CCS Expressions在线修改；DSP复位后由Measurement_Init()重新加载
 * board_config.h中的对应*_OFFSET_COUNTS_DEFAULT。
 * 零偏只用于修正零输入码值，禁止用来补偿CT1/CT2/Gain等比例误差。
 */
extern volatile uint16_t g_vdc1_offset_counts;
extern volatile uint16_t g_vdc2_offset_counts;
extern volatile uint16_t g_vdc3_offset_counts;
extern volatile uint16_t g_vdc4_offset_counts;
extern volatile uint16_t g_vdc5_offset_counts;
extern volatile uint16_t g_vdc6_offset_counts;
extern volatile uint16_t g_vac_va_offset_counts;
extern volatile uint16_t g_vac_vb_offset_counts;
extern volatile uint16_t g_vac_vc_offset_counts;
extern volatile uint16_t g_iac_ia_offset_counts;
extern volatile uint16_t g_iac_ib_offset_counts;
extern volatile uint16_t g_iac_ic_offset_counts;

/* 上电加载各通道DEFAULT零偏；不执行自动零偏测量。 */
void Measurement_Init(void);

/*
 * 可复用的单通道换算函数 — 纯函数、无状态。
 * 后续 20 kHz 快速控制路径可直接调用，无需经过 Measurement_Update()。
 */

/* Vdc: V_primary = max(raw-offset,0) × VREF/MAX_COUNT × CT2比例 × CT1比例 / GAIN。 */
float Measurement_ConvertVdc(uint16_t raw, uint16_t offset);

/*
 * Vac: delta = (int32_t)raw - offset，每相使用独立offset运行变量
 *      V_primary = polarity × delta × VREF/MAX_COUNT / (TIA×GAIN)
 *                × CT2_PRI_V/CT2_SEC_A × CT1_PRI_V/CT1_SEC_V
 */
float Measurement_ConvertVac(uint16_t raw, uint16_t offset, float polarity);

/*
 * Iac: delta = (int32_t)raw - offset，每相使用独立offset运行变量
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
