#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include "firmware/services/measurement.h"

volatile uint16_t g_vdc_raw[6];
volatile uint16_t g_vac_raw[3];
volatile uint16_t g_iac_raw[3];
volatile uint32_t g_adc_frame_count;

static int g_failures;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); g_failures++; } } while (0)

int main(void)
{
    MeasurementSample sample;
    float vdc_one_count;

    printf("=== Measurement Independent Offset Tests ===\n");
    Measurement_Init();

    CHECK(g_vdc1_offset_counts == 0U && g_vdc6_offset_counts == 0U,
          "Vdc offset defaults loaded independently");
    CHECK(g_vac_va_offset_counts == 2048U && g_iac_ic_offset_counts == 2048U,
          "Vac/Iac offset defaults loaded");

    vdc_one_count = Measurement_ConvertVdc(1U, 0U);
    CHECK(Measurement_ConvertVdc(99U, 100U) == 0.0f,
          "Vdc raw below offset saturates to zero");
    CHECK(Measurement_ConvertVdc(100U, 100U) == 0.0f,
          "Vdc raw equal offset is zero");
    CHECK(fabsf(Measurement_ConvertVdc(101U, 100U) - vdc_one_count) < 1.0e-7f,
          "Vdc uses max(raw-offset,0) times scale");
    CHECK(fabsf(vdc_one_count - 0.73260f) < 0.0001f,
          "Vdc CT1 1000:2 scale is about 0.73260 V/count");
    CHECK(fabsf(Measurement_ConvertVdc(546U, 0U) - 400.0f) < 0.1f,
          "Vdc CT1 1000:2: corrected raw 546 equals about 400 V");

    CHECK(fabsf(Measurement_ConvertVac(2049U, 2048U, 1.0f) - 0.08774f) < 0.0001f,
          "Vac CT1 1:1 scale is about 0.08774 V/count");
    CHECK(fabsf(Measurement_ConvertIac(2049U, 2048U, 1.0f) - 0.0029304f) < 0.00001f,
          "Iac CT1 1:1 scale is about 0.0029304 A/count");

    CHECK(Measurement_ConvertVac(2047U, 2048U, 1.0f) < 0.0f,
          "Vac keeps signed raw-offset result");
    CHECK(Measurement_ConvertIac(2047U, 2048U, 1.0f) < 0.0f,
          "Iac keeps signed raw-offset result");

    g_vdc1_offset_counts = 10U;
    g_vdc2_offset_counts = 20U;
    g_vdc_raw[0] = 11U;
    g_vdc_raw[1] = 22U;
    Measurement_Update(&sample);
    CHECK(fabsf(sample.vdc_v[0] - vdc_one_count) < 1.0e-7f,
          "Vdc1 uses its own runtime offset");
    CHECK(fabsf(sample.vdc_v[1] - 2.0f * vdc_one_count) < 1.0e-7f,
          "Vdc2 uses its own runtime offset");

    printf("=== %s ===\n", g_failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED");
    return g_failures ? 1 : 0;
}
