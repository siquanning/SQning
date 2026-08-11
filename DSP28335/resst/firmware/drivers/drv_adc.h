#ifndef DRV_ADC_H
#define DRV_ADC_H

#include <stdint.h>

typedef struct
{
    uint16_t adcclkps;       /* ADCTRL3.ADCCLKPS divider (0-15) */
    uint16_t acq_ps;         /* ADCTRL1.ACQ_PS acquisition window */
    uint16_t cps;            /* ADCTRL1.CPS core clock prescaler */
    uint16_t num_channels;   /* 1-16 */
    uint16_t channels[16];   /* Channel select values (0-15) */
} DrvAdcConfig;

/*
 * Initialize ADC to powered-up, configured, but NOT triggered state.
 * Powers bandgap/ref → ADC core → software reset → configure channels.
 * After return, EPWM_SOCA_SEQ1 and INT_ENA_SEQ1 are still disabled.
 * Caller enables them via ADCTRL2 after ePWM + GPIO mux are confirmed safe.
 *
 * Returns 0 on success, -1 on invalid config.
 */
int32_t DrvAdc_Init(const DrvAdcConfig *cfg);

/*
 * Read raw ADC result mirror register (0-4095). Non-blocking.
 * channel = 0..15 maps to ADCRESULT0..ADCRESULT15.
 * Uses the zero-wait-state, right-justified PF0 mirror window.
 * Returns negative on invalid channel.
 */
int32_t DrvAdc_ReadRaw(uint32_t channel);

/*
 * Enable hardware triggering and interrupt generation for SEQ1.
 * Sets EPWM_SOCA_SEQ1 and INT_ENA_SEQ1 in ADCTRL2.
 * Must only be called after both ADC and ePWM drivers are fully initialized
 * and GPIO mux for ePWM/TZ is confirmed safe.
 */
void DrvAdc_EnableTrigger(void);

/* Clear SEQ1 interrupt flag. Call from ADC ISR. */
void DrvAdc_ClearInterrupt(void);

/* Reset the cascaded sequencer result pointer after an end-of-sequence ISR. */
void DrvAdc_ResetSequencer(void);

/* Acknowledge ADC interrupt in PIE group 1. Call from ADC ISR. */
void DrvAdc_AckInterrupt(void);

#endif
