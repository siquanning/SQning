/* Created by Siquanning */
#include "firmware/services/measurement.h"
#include "firmware/bsp/board_config.h"
#include "firmware/app/isr.h"

/*
 * ====================================================================
 * Measurement — raw ADC → instantaneous physical quantity conversion.
 *
 * All gain factors are derived from board_config.h hardware-chain macros.
 * No hand-filled magic numbers in this file.
 *
 * Conversion formulas (see board_config.h for full analog signal chain):
 *
 *   Vdc: V_primary = max(raw - offset, 0) × (VREF/MAX_COUNT)
 *                  × (CT2_PRI_V/CT2_SEC_V)
 *                  × (CT1_PRI_V/CT1_SEC_V) / ANALOG_GAIN
 *
 *   Vac: delta     = (int32_t)raw - (int32_t)OFFSET
 *        V_primary = POLARITY × delta
 *                  × (VREF/MAX_COUNT) / (TIA_OHM × ANALOG_GAIN)
 *                  × (CT2_PRI_V_RMS/CT2_SEC_A_RMS)
 *                  × (CT1_PRI_V_RMS/CT1_SEC_V_RMS)
 *
 *   Iac: delta     = (int32_t)raw - (int32_t)OFFSET
 *        I_primary = POLARITY × delta
 *                  × (VREF/MAX_COUNT) / (TIA_OHM × ANALOG_GAIN)
 *                  × (CT2_PRI_A/CT2_SEC_A)
 *                  × (CT1_PRI_A/CT1_SEC_A)
 * ====================================================================
 */

/* Global measurement snapshot — 1 kHz refresh, CCS watchable. */
MeasurementSample g_measurement;

/* 各通道独立ADC零偏运行值，单位ADC count；可在CCS Expressions现场修改。 */
volatile uint16_t g_vdc1_offset_counts;
volatile uint16_t g_vdc2_offset_counts;
volatile uint16_t g_vdc3_offset_counts;
volatile uint16_t g_vdc4_offset_counts;
volatile uint16_t g_vdc5_offset_counts;
volatile uint16_t g_vdc6_offset_counts;
volatile uint16_t g_vac_vab_offset_counts;
volatile uint16_t g_vac_vbc_offset_counts;
volatile uint16_t g_vac_vca_offset_counts;
volatile uint16_t g_iac_ia_offset_counts;
volatile uint16_t g_iac_ib_offset_counts;
volatile uint16_t g_iac_ic_offset_counts;

void Measurement_Init(void)
{
    /*
     * 这里只加载人工确认的默认零偏，不做自动校零。
     * 在不能保证输入确实为0时，自动采样会把真实信号误当成偏置。
     */
    g_vdc1_offset_counts = BOARD_VDC1_OFFSET_COUNTS_DEFAULT;
    g_vdc2_offset_counts = BOARD_VDC2_OFFSET_COUNTS_DEFAULT;
    g_vdc3_offset_counts = BOARD_VDC3_OFFSET_COUNTS_DEFAULT;
    g_vdc4_offset_counts = BOARD_VDC4_OFFSET_COUNTS_DEFAULT;
    g_vdc5_offset_counts = BOARD_VDC5_OFFSET_COUNTS_DEFAULT;
    g_vdc6_offset_counts = BOARD_VDC6_OFFSET_COUNTS_DEFAULT;
    g_vac_vab_offset_counts = BOARD_VAC_VAB_OFFSET_COUNTS_DEFAULT;
    g_vac_vbc_offset_counts = BOARD_VAC_VBC_OFFSET_COUNTS_DEFAULT;
    g_vac_vca_offset_counts = BOARD_VAC_VCA_OFFSET_COUNTS_DEFAULT;
    g_iac_ia_offset_counts = BOARD_IAC_IA_OFFSET_COUNTS_DEFAULT;
    g_iac_ib_offset_counts = BOARD_IAC_IB_OFFSET_COUNTS_DEFAULT;
    g_iac_ic_offset_counts = BOARD_IAC_IC_OFFSET_COUNTS_DEFAULT;
}

/*
 * Derived gain factors — computed from board_config.h hardware-chain macros.
 *
 * Theoretical check values at current parameters (not hard-coded, for
 * compile-time verification only):
 *   VDC_GAIN ≈ 0.366300 V/count；corrected_raw=546时约200V（Vdc CT1=1000V:2V，模拟增益1:1）
 *   VAC_GAIN ≈ 0.08774 V/count (× delta)
 *   IAC_GAIN ≈ 0.058608 A/count (× delta)  (Iac CT1 = 100A:5A)
 */

/* Vdc scaling: VREF/MAX_COUNT × CT ratios / gain */
#define MEAS_VDC_SCALE  ((BOARD_ADC_VREF_V / BOARD_ADC_MAX_COUNT) \
                        * (BOARD_VDC_CT2_PRI_V / BOARD_VDC_CT2_SEC_V) \
                        * (BOARD_VDC_CT1_PRI_V / BOARD_VDC_CT1_SEC_V) \
                        / BOARD_VDC_ANALOG_GAIN)

/* Vac/Iac common ADC step: VREF / MAX_COUNT / (TIA × ANALOG_GAIN) */
#define MEAS_VAC_ADC_STEP  ((BOARD_ADC_VREF_V / BOARD_ADC_MAX_COUNT) \
                           / (BOARD_VAC_TIA_OHM * BOARD_VAC_ANALOG_GAIN))
#define MEAS_IAC_ADC_STEP  ((BOARD_ADC_VREF_V / BOARD_ADC_MAX_COUNT) \
                           / (BOARD_IAC_TIA_OHM * BOARD_IAC_ANALOG_GAIN))

/* Vac scaling: ADC step × CT2_V/CT2_A × CT1_V/CT1_V */
#define MEAS_VAC_SCALE  (MEAS_VAC_ADC_STEP \
                        * (BOARD_VAC_CT2_PRI_V_RMS / BOARD_VAC_CT2_SEC_A_RMS) \
                        * (BOARD_VAC_CT1_PRI_V_RMS / BOARD_VAC_CT1_SEC_V_RMS))

/* Iac scaling: ADC step × CT2_A/CT2_A × CT1_A/CT1_A */
#define MEAS_IAC_SCALE  (MEAS_IAC_ADC_STEP \
                        * (BOARD_IAC_CT2_PRI_A / BOARD_IAC_CT2_SEC_A) \
                        * (BOARD_IAC_CT1_PRI_A / BOARD_IAC_CT1_SEC_A))

/* ====================================================================
 * Reusable per-channel conversion — pure functions, callable from any
 * context (1 kHz foreground or 20 kHz ISR).
 * ==================================================================== */

float Measurement_ConvertVdc(uint16_t raw, uint16_t offset)
{
    /* Vdc为单极性量；raw低于offset时饱和为0，避免产生虚假负母线电压。 */
    uint16_t corrected = (raw > offset) ? (uint16_t)(raw - offset) : 0U;
    return (float)corrected * MEAS_VDC_SCALE;
}

float Measurement_ConvertVac(uint16_t raw, uint16_t offset, float polarity)
{
    int32_t delta = (int32_t)raw - (int32_t)offset;
    return polarity * (float)delta * MEAS_VAC_SCALE;
}

void Measurement_LineToPhase(float vab, float vbc, float vca,
                             float *va, float *vb, float *vc)
{
    const float one_third = 1.0f / 3.0f;
    if ((va == ((float *)0)) || (vb == ((float *)0)) ||
        (vc == ((float *)0)))
        return;
    *va = (2.0f * vab + vbc) * one_third;
    *vb = (2.0f * vbc + vca) * one_third;
    *vc = (2.0f * vca + vab) * one_third;
}

float Measurement_ConvertIac(uint16_t raw, uint16_t offset, float polarity)
{
    int32_t delta = (int32_t)raw - (int32_t)offset;
    return polarity * (float)delta * MEAS_IAC_SCALE;
}

/* ====================================================================
 * Measurement_Update — 1 kHz batch snapshot.
 *
 * Reads the ADC ISR globals (g_vdc_raw/g_vac_raw/g_iac_raw) through
 * the reusable conversion functions.  Does NOT write to any raw global
 * or affect Modbus HR0~11.
 * ==================================================================== */

void Measurement_Update(MeasurementSample *out)
{
    if (out == ((MeasurementSample *)0))
        return;

    /* Vdc：每路使用独立运行offset，按max(raw-offset,0)换算。 */
    out->vdc_v[0] = Measurement_ConvertVdc(g_vdc_raw[0], g_vdc1_offset_counts);
    out->vdc_v[1] = Measurement_ConvertVdc(g_vdc_raw[1], g_vdc2_offset_counts);
    out->vdc_v[2] = Measurement_ConvertVdc(g_vdc_raw[2], g_vdc3_offset_counts);
    out->vdc_v[3] = Measurement_ConvertVdc(g_vdc_raw[3], g_vdc4_offset_counts);
    out->vdc_v[4] = Measurement_ConvertVdc(g_vdc_raw[4], g_vdc5_offset_counts);
    out->vdc_v[5] = Measurement_ConvertVdc(g_vdc_raw[5], g_vdc6_offset_counts);

    /* 零偏减在线电压 ADC 上；再重构相电压供 Watch / 与控制同源 */
    out->vline_v[0] = Measurement_ConvertVac(g_vac_raw[0],
                    g_vac_vab_offset_counts, BOARD_VAC_VAB_POLARITY);
    out->vline_v[1] = Measurement_ConvertVac(g_vac_raw[1],
                    g_vac_vbc_offset_counts, BOARD_VAC_VBC_POLARITY);
    out->vline_v[2] = Measurement_ConvertVac(g_vac_raw[2],
                    g_vac_vca_offset_counts, BOARD_VAC_VCA_POLARITY);
    Measurement_LineToPhase(out->vline_v[0], out->vline_v[1], out->vline_v[2],
                            &out->vac_v[0], &out->vac_v[1], &out->vac_v[2]);

    /* Iac — bipolar, offset-corrected, instantaneous (not RMS) */
    out->iac_a[0] = Measurement_ConvertIac(g_iac_raw[0],
                    g_iac_ia_offset_counts, BOARD_IAC_IA_POLARITY);
    out->iac_a[1] = Measurement_ConvertIac(g_iac_raw[1],
                    g_iac_ib_offset_counts, BOARD_IAC_IB_POLARITY);
    out->iac_a[2] = Measurement_ConvertIac(g_iac_raw[2],
                    g_iac_ic_offset_counts, BOARD_IAC_IC_POLARITY);
}
