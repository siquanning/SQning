#include "control/iir.h"

void iir1_init(iir1_t *f, float alpha)
{
    f->alpha = alpha;
    f->y_prev = 0.0f;
}

float iir1_step(iir1_t *f, float x)
{
    float y = f->alpha * x + (1.0f - f->alpha) * f->y_prev;
    f->y_prev = y;
    return y;
}
