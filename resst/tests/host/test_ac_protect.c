#include <stdio.h>
#include "firmware/app/ac_protect.h"
#include "firmware/bsp/board_config.h"

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", (m)); failures++; } } while (0)

static void clear_inputs(float vline[3], float iac[3], float vdc[6])
{
    uint16_t i;
    for (i = 0U; i < 3U; i++) {
        vline[i] = 0.0f;
        iac[i] = 0.0f;
    }
    for (i = 0U; i < 6U; i++) vdc[i] = 0.0f;
}

int main(void)
{
    float vline[3], iac[3], vdc[6];

    printf("=== AC/DC Protect Tests ===\n");
    clear_inputs(vline, iac, vdc);
    CHECK(AcProtect_Check(vline, iac, vdc) == FAULT_NONE, "zero inputs are safe");

    vline[0] = 99.9f;
    CHECK(AcProtect_Check(vline, iac, vdc) == FAULT_NONE, "99.9V line does not trip");
    vline[0] = 100.1f;
    CHECK(AcProtect_Check(vline, iac, vdc) == FAULT_HW_AC_OVERVOLTAGE,
          "100.1V line trips AC overvoltage");
    vline[0] = -100.1f;
    CHECK(AcProtect_Check(vline, iac, vdc) == FAULT_HW_AC_OVERVOLTAGE,
          "negative 100.1V line trips AC overvoltage");

    clear_inputs(vline, iac, vdc);
    iac[1] = 9.9f;
    CHECK(AcProtect_Check(vline, iac, vdc) == FAULT_NONE, "9.9A does not trip");
    iac[1] = 10.1f;
    CHECK(AcProtect_Check(vline, iac, vdc) == FAULT_HW_AC_OVERCURRENT,
          "10.1A trips AC overcurrent");
    iac[1] = -10.1f;
    CHECK(AcProtect_Check(vline, iac, vdc) == FAULT_HW_AC_OVERCURRENT,
          "negative 10.1A trips AC overcurrent");

    clear_inputs(vline, iac, vdc);
    vline[2] = 200.0f;
    iac[0] = 20.0f;
    vdc[0] = 90.0f;
    CHECK(AcProtect_Check(vline, iac, vdc) == FAULT_HW_AC_OVERVOLTAGE,
          "AC overvoltage has priority over OC and DC OV");

    clear_inputs(vline, iac, vdc);
    vdc[3] = 80.1f;
    CHECK(AcProtect_Check(vline, iac, vdc) == FAULT_HW_DC_OVERVOLTAGE,
          "single capacitor 80.1V trips DC overvoltage");
    vdc[3] = 0.0f;
    vdc[0] = 50.0f;
    vdc[1] = 50.0f;
    CHECK(AcProtect_Check(vline, iac, vdc) == FAULT_NONE,
          "vdc_avg 50V does not trip");
    vdc[0] = 81.0f;
    vdc[1] = 81.0f;
    CHECK(AcProtect_Check(vline, iac, vdc) == FAULT_HW_DC_OVERVOLTAGE,
          "vdc_avg 81V trips DC overvoltage");

    CHECK(AcProtect_Check(((const float *)0), iac, vdc) == FAULT_NONE,
          "NULL vline is ignored");

    printf("=== %s ===\n", failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED");
    return failures ? 1 : 0;
}
