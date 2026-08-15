#include "firmware/control/control_openloop.h"
#include <math.h>

/*
 * 1200-point Q15 sine LUT.  sin_table[i] ≈ 32767 × sin(2π·i/1200).
 * Generated once at init via FPU, then ISR only does integer table lookups.
 */
static int16_t sin_table[OPENLOOP_LUT_SIZE];
static uint16_t phase_index;

void OpenLoop_InitSine(void)
{
    uint16_t i;
    for (i = 0U; i < OPENLOOP_LUT_SIZE; i++)
    {
        float angle = 2.0f * 3.141592653589793f
                    * (float)i / (float)OPENLOOP_LUT_SIZE;
        sin_table[i] = (int16_t)(32767.0f * sinf(angle));
    }
    phase_index = 0U;
}

/*
 * 3-phase 50 Hz sine reference generator.
 *
 *    1200 / 3 = 400 ISR calls per fundamental cycle
 *    20000 Hz / 400 = 50 Hz
 *
 * Phase offsets (mod 1200):
 *   A: index + 0     = index          → sin(θ)
 *   B: index + 800   = index - 400    → sin(θ - 120°)
 *   C: index + 400   = index + 400    → sin(θ + 120°)
 */
void OpenLoop_GenerateSine(int16_t mabc[3])
{
    uint16_t idx_a, idx_b, idx_c;
    int32_t temp;

    idx_a = phase_index;

    idx_b = phase_index + 800U;   /* -400 mod 1200 → θ - 120° */
    if (idx_b >= OPENLOOP_LUT_SIZE)
        idx_b -= OPENLOOP_LUT_SIZE;

    idx_c = phase_index + 400U;   /* θ + 120° */
    if (idx_c >= OPENLOOP_LUT_SIZE)
        idx_c -= OPENLOOP_LUT_SIZE;

    temp   = (int32_t)OPEN_LOOP_M_PERMILL_A * (int32_t)sin_table[idx_a];
    mabc[0] = (int16_t)(temp / 32767L);

    temp   = (int32_t)OPEN_LOOP_M_PERMILL_B * (int32_t)sin_table[idx_b];
    mabc[1] = (int16_t)(temp / 32767L);

    temp   = (int32_t)OPEN_LOOP_M_PERMILL_C * (int32_t)sin_table[idx_c];
    mabc[2] = (int16_t)(temp / 32767L);

    phase_index += OPENLOOP_LUT_STEP;
    if (phase_index >= OPENLOOP_LUT_SIZE)
        phase_index -= OPENLOOP_LUT_SIZE;
}

uint16_t OpenLoop_GetPhaseIndex(void)
{
    return phase_index;
}
