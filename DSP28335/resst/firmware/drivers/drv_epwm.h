#ifndef DRV_EPWM_H
#define DRV_EPWM_H

#include <stdint.h>

/* ---- Configuration structure ---- */
typedef struct
{
    uint32_t tbclk_hz;
    uint32_t pwm_freq_hz;
    uint16_t count_mode;
    uint16_t db_red;
    uint16_t db_fed;
    uint16_t tz_sources;
    uint16_t tz_oneshot_enable;
    uint16_t tz_cbc_enable;
} DrvEpwmConfig;

/* ---- ePWM / Trip Zone API ---- */

/*
 * Initialize ePWM module + Trip Zone to SAFE DISABLED state.
 * - All registers configured for safe operation.
 * - AQCSFRC forces EPWMxA/B LOW (CSFA=1, CSFB=1).
 * - TZ configured with TZ_FORCE_LO on both outputs.
 * - TZ flags cleared.
 * - ETSEL SOCA configured but not yet enabled at ADC side.
 * - TBCTR frozen (TBCLKSYNC=0 must already be set by caller).
 * - GPIO mux NOT set (caller controls via BSP).
 *
 * module: 1-6 (which ePWM peripheral).
 * Returns 0 on success, negative on invalid parameter.
 */
int32_t DrvEpwm_Init(uint32_t module, const DrvEpwmConfig *cfg);

/*
 * Halt ALL ePWM time-base clocks by setting TBCLKSYNC=0.
 * Must be called before any ePWM register initialization.
 * This is the primary hardware init safety gate.
 */
void DrvEpwm_HaltTimebase(void);

/*
 * Configure GPIO mux for the specified ePWM module.
 * Sets EPWMxA and EPWMxB pins, plus TZ1/TZ2 on first call.
 * This is gated by BOARD_PWM_ADC_HW_CONFIRMED — no-op when 0.
 */
void DrvEpwm_ConfigGpio(uint32_t module);

/*
 * Write CMPA shadow register. Value clamped to [0, TBPRD * max_duty_permill / 1000].
 * max_duty_permill: hard upper limit in per-mill (e.g., 480 = 48.0%).
 * Shadow load occurs at configured load point (CTR=ZERO by default).
 */
void DrvEpwm_SetCompareA(uint32_t module, uint16_t value,
                         uint16_t max_duty_permill);

/*
 * Write CMPB shadow register. Clamped same as CMPA.
 */
void DrvEpwm_SetCompareB(uint32_t module, uint16_t value,
                         uint16_t max_duty_permill);

/*
 * Set per-leg continuous force HIGH mode for clamped-unipolar PWM.
 * Writes both CSFA and CSFB in a single AQCSFRC shadow write;
 * shadow loads at CTR=ZERO (synchronized with CMP shadow load).
 * force_a/force_b: 1 = continuous force HIGH, 0 = release to normal PWM.
 *
 * DEPRECATED for H1 modulation — use DrvEpwm_SetHalfBridgeForceHigh()
 * for the new 6-module half-bridge architecture.
 */
void DrvEpwm_SetLegForceHighPair(uint32_t module, uint16_t force_a,
                                  uint16_t force_b);

/*
 * Set half-bridge force-HIGH mode (6-ePWM half-bridge architecture).
 *
 *   force_high=1 → EPWMxA=HIGH, EPWMxB=LOW  (AQCSFRC=0x0006)
 *   force_high=0 → release, AQCTLA/AQCTLB generate complementary PWM
 *
 * Shadow loads at CTR=ZERO via AQSFRC.RLDCSF=0 (set in DrvEpwm_Init).
 * module: 1-6 (which ePWM peripheral).
 */
void DrvEpwm_SetHalfBridgeForceHigh(uint32_t module, uint16_t force_high);

/*
 * Enable ePWM period interrupt (INT at CTR=ZERO, every event).
 * Sets ETSEL.INTSEL=CTR_ZERO, ETSEL.INTEN=1, ETPS.INTPRD=1ST.
 * Call after PWM_StartTimebase() — the ISR fires at the next CTR=ZERO.
 */
void DrvEpwm_EnablePeriodInt(uint32_t module);

/* Clear ETFLG.INT flag. Called from ISR after processing. */
void DrvEpwm_ClearIntFlag(uint32_t module);

/*
 * Enable ADC SOCA output on the specified ePWM module.
 * Precondition: DrvEpwm_Init already configured SOCASEL (=ET_CTR_ZERO).
 * Call after ADC ISR is bound, before TBCLKSYNC=1.
 */
void DrvEpwm_EnableAdcSocA(uint32_t module);

/*
 * Force one-shot TZ trip to block PWM outputs (safety-only — never touches AQCSFRC).
 * TZ action = TZ_FORCE_LO on both outputs. Latch persists until ClearOstTrip.
 */
void DrvEpwm_ForceTrip(uint32_t module);

/* ---- Trip Zone interface ---- */

/* Get TZFLG register value (active trip flags). */
uint16_t DrvEpwm_GetTripStatus(uint32_t module);

/* Clear cycle-by-cycle (CBC) trip latch. */
void DrvEpwm_ClearCbcTrip(uint32_t module);

/*
 * Clear one-shot (OST) trip latch.
 * WARNING: Only call after fault condition is resolved and safety
 * preconditions are re-validated. This does NOT re-enable PWM —
 * caller must separately call DrvEpwm_Enable.
 */
void DrvEpwm_ClearOstTrip(uint32_t module);

/* Read TBPRD (period register). */
uint16_t DrvEpwm_GetPeriod(uint32_t module);

/* Read TBCTR (counter, for diagnostic use only). */
uint16_t DrvEpwm_GetCounter(uint32_t module);

#endif
