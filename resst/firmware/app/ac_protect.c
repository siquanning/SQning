/* Created by Siquanning */
#include "firmware/app/ac_protect.h"
#include "firmware/bsp/board_config.h"

static float absf_local(float x)
{
    return (x < 0.0f) ? -x : x;
}

SystemFault AcProtect_Check(const float vline[3],
                            const float iac[3],
                            const float vdc[6])
{
    uint16_t i;

    if ((vline == ((const float *)0)) ||
        (iac == ((const float *)0)) ||
        (vdc == ((const float *)0))) {
        return FAULT_NONE;
    }

    for (i = 0U; i < 3U; i++) {
        if (absf_local(vline[i]) > BOARD_AC_OVERVOLTAGE_V)
            return FAULT_HW_AC_OVERVOLTAGE;
    }

    for (i = 0U; i < 3U; i++) {
        if (absf_local(iac[i]) > BOARD_AC_OVERCURRENT_A)
            return FAULT_HW_AC_OVERCURRENT;
    }

    for (i = 0U; i < 6U; i++) {
        if (vdc[i] > BOARD_DC_OVERVOLTAGE_V)
            return FAULT_HW_DC_OVERVOLTAGE;
    }
    for (i = 0U; i < 3U; i++) {
        float vdc_avg = 0.5f * (vdc[2U * i] + vdc[2U * i + 1U]);
        if (vdc_avg > BOARD_DC_OVERVOLTAGE_V)
            return FAULT_HW_DC_OVERVOLTAGE;
    }

    return FAULT_NONE;
}
