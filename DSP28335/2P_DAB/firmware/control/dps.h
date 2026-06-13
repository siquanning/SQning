#ifndef CONTROL_DPS_H
#define CONTROL_DPS_H

// DPS (Dual Phase Shift) modulation algorithm for DAB converter.
// Minimizes RMS current for given power command p0 and voltage ratio k.

// Compute D1 (inter-bridge) and D2 (intra-bridge) phase shifts.
// p0: normalized power [0, 1],  k: voltage ratio V1/(n*V2)
void dps_compute(float p0, float k, float *d1, float *d2);

// Compute PLECS-equivalent modulation signals from D1, D2.
// SD1 controls S3/S4, SD2 controls Q1/Q2, ST controls Q3/Q4.
void dps_modulation_signals(float d1, float d2,
                            float *sd1, float *sd2, float *st);

// Apply D1, D2 to ePWM1~4 TBPHS registers.
// ePWM1=S1/S2 (ref=0), ePWM2=S3/S4 (D2), ePWM3=Q1/Q2 (D1), ePWM4=Q3/Q4 (D1+D2)
void dps_update_epwm(float d1, float d2);

#endif
