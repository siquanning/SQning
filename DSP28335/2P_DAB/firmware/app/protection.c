#include "include/common.h"
#include "drivers/epwm.h"
#include "drivers/adc.h"
#include "app/state_machine.h"
#include "app/protection.h"
#include "bsp/led.h"

// Protection thresholds — CCS debugger configurable
float g_ovp_threshold = 120.0f;
float g_ocp_threshold = 15.0f;

// Fault latch
volatile int g_prot_fault       = 0;
volatile int g_prot_fault_clear = 0;

// Anti-glitch counters
volatile int g_ovp_count = 0;
volatile int g_ocp_count = 0;

// Current measurement placeholder — drive from current-sensor ADC ISR
float g_i_measured = 0.0f;

#define CONSECUTIVE_THRESHOLD 3

// Forward declarations
static void tz_software_trip(void);
static void tz_clear(void);

static void tz_software_trip(void)
{
    EALLOW;
    EPwm1Regs.TZFRC.bit.OST = 1;
    EPwm2Regs.TZFRC.bit.OST = 1;
    EPwm3Regs.TZFRC.bit.OST = 1;
    EPwm4Regs.TZFRC.bit.OST = 1;
    EDIS;
}

static void tz_clear(void)
{
    // Clear OST trip latch on all 4 ePWM modules
    EALLOW;
    EPwm1Regs.TZCLR.bit.OST = 1;
    EPwm2Regs.TZCLR.bit.OST = 1;
    EPwm3Regs.TZCLR.bit.OST = 1;
    EPwm4Regs.TZCLR.bit.OST = 1;
    // Also clear any software-forced trip
    EPwm1Regs.TZFRC.bit.OST = 0;
    EPwm2Regs.TZFRC.bit.OST = 0;
    EPwm3Regs.TZFRC.bit.OST = 0;
    EPwm4Regs.TZFRC.bit.OST = 0;
    EDIS;
}

void protection_init(void)
{
    // GPIO12 → TZ1 is done in gpio_init().
    // TZ select/control per ePWM is done in epwm_tz_init() called by epwm_init().

    g_prot_fault       = 0;
    g_prot_fault_clear = 0;
    g_ovp_count        = 0;
    g_ocp_count        = 0;
    led3_off();
}

void protection_step(void)
{
    // ---- 1. Fault clear request (Modbus write entry point) ----
    if (g_prot_fault_clear) {
        g_prot_fault_clear = 0;
        g_prot_fault       = 0;
        g_ovp_count        = 0;
        g_ocp_count        = 0;
        g_fault_flag       = 0;
        g_system_state     = STATE_IDLE;
        tz_clear();
        led3_off();
        return;
    }

    // ---- 2. Already latched — hold fault, keep LED3 solid ----
    if (g_prot_fault) {
        led3_on();
        // Keep pushing state into FAULT in case something else
        // (e.g. g_stop_cmd) pulled us out of it
        if (g_system_state != STATE_FAULT) {
            g_system_state = STATE_FAULT;
            g_fault_flag   = 1;
        }
        return;
    }

    // ---- 3. Check hardware TZ1 flag (GPIO12 hardware trip) ----
    if (EPwm1Regs.TZFLG.bit.OST || EPwm2Regs.TZFLG.bit.OST ||
        EPwm3Regs.TZFLG.bit.OST || EPwm4Regs.TZFLG.bit.OST) {
        g_prot_fault   = 1;
        g_fault_flag   = 1;
        g_system_state = STATE_FAULT;
        led3_on();
        return;
    }

    // ---- 4. OVP check: 3-consecutive-sample anti-glitch ----
    if (g_adc.v2_filtered > g_ovp_threshold) {
        g_ovp_count++;
        if (g_ovp_count >= CONSECUTIVE_THRESHOLD) {
            g_prot_fault   = 1;
            g_fault_flag   = 1;
            g_system_state = STATE_FAULT;
            tz_software_trip();
            led3_on();
            return;
        }
    } else {
        g_ovp_count = 0;
    }

    // ---- 5. OCP check: 3-consecutive-sample anti-glitch ----
    if (g_i_measured > g_ocp_threshold) {
        g_ocp_count++;
        if (g_ocp_count >= CONSECUTIVE_THRESHOLD) {
            g_prot_fault   = 1;
            g_fault_flag   = 1;
            g_system_state = STATE_FAULT;
            tz_software_trip();
            led3_on();
            return;
        }
    } else {
        g_ocp_count = 0;
    }
}
