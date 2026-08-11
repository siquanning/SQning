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
 *   Vdc: V_primary = raw × (VREF/MAX_COUNT)
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

/*
 * Derived gain factors — computed from board_config.h hardware-chain macros.
 *
 * Theoretical check values at current parameters (not hard-coded, for
 * compile-time verification only):
 *   VDC_GAIN ≈ 0.0007326 V/count
 *   VAC_GAIN ≈ 0.08774 V/count (× delta)
 *   IAC_GAIN ≈ 0.0029304 A/count (× delta)
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

float Measurement_ConvertVdc(uint16_t raw)
{
    return (float)raw * MEAS_VDC_SCALE;
}

float Measurement_ConvertVac(uint16_t raw, uint16_t offset, float polarity)
{
    int32_t delta = (int32_t)raw - (int32_t)offset;
    return polarity * (float)delta * MEAS_VAC_SCALE;
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

    /* Vdc — unsigned, no offset */
    out->vdc_v[0] = Measurement_ConvertVdc(g_vdc_raw[0]);
    out->vdc_v[1] = Measurement_ConvertVdc(g_vdc_raw[1]);
    out->vdc_v[2] = Measurement_ConvertVdc(g_vdc_raw[2]);
    out->vdc_v[3] = Measurement_ConvertVdc(g_vdc_raw[3]);
    out->vdc_v[4] = Measurement_ConvertVdc(g_vdc_raw[4]);
    out->vdc_v[5] = Measurement_ConvertVdc(g_vdc_raw[5]);

    /* Vac — bipolar, offset-corrected, instantaneous (not RMS) */
    out->vac_v[0] = Measurement_ConvertVac(g_vac_raw[0],
                    BOARD_VAC_VA_OFFSET_COUNTS, BOARD_VAC_VA_POLARITY);
    out->vac_v[1] = Measurement_ConvertVac(g_vac_raw[1],
                    BOARD_VAC_VB_OFFSET_COUNTS, BOARD_VAC_VB_POLARITY);
    out->vac_v[2] = Measurement_ConvertVac(g_vac_raw[2],
                    BOARD_VAC_VC_OFFSET_COUNTS, BOARD_VAC_VC_POLARITY);

    /* Iac — bipolar, offset-corrected, instantaneous (not RMS) */
    out->iac_a[0] = Measurement_ConvertIac(g_iac_raw[0],
                    BOARD_IAC_IA_OFFSET_COUNTS, BOARD_IAC_IA_POLARITY);
    out->iac_a[1] = Measurement_ConvertIac(g_iac_raw[1],
                    BOARD_IAC_IB_OFFSET_COUNTS, BOARD_IAC_IB_POLARITY);
    out->iac_a[2] = Measurement_ConvertIac(g_iac_raw[2],
                    BOARD_IAC_IC_OFFSET_COUNTS, BOARD_IAC_IC_POLARITY);
}
