#ifndef APP_PROTECTION_H
#define APP_PROTECTION_H

#include "include/common.h"

// Protection thresholds — CCS debugger configurable
extern float g_ovp_threshold;       // Over-Voltage Protection, default 120V
extern float g_ocp_threshold;       // Over-Current Protection, default 15A

// Fault latch
extern volatile int g_prot_fault;        // 1 = protection fault latched
extern volatile int g_prot_fault_clear;  // Set 1 to clear latched fault (Modbus)

// Anti-glitch counters: 3 consecutive over-threshold samples to trigger
extern volatile int g_ovp_count;
extern volatile int g_ocp_count;

// Current measurement — feed from ADC ISR when current sensor is wired
// Defaults to 0.0f, OCP won't trigger until this is driven by hardware.
extern float g_i_measured;

// Init: GPIO12 → TZ1, ePWM trip-zone config
void protection_init(void);

// Called every 1ms by control_step: check OVP/OCP, manage TZ latch
void protection_step(void);

#endif
