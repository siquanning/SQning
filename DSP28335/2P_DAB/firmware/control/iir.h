#ifndef CONTROL_IIR_H
#define CONTROL_IIR_H

// First-order IIR low-pass filter: y[n] = alpha * x[n] + (1 - alpha) * y[n-1]
typedef struct {
    float alpha;    // filter coefficient [0, 1]
    float y_prev;   // previous output
} iir1_t;

void iir1_init(iir1_t *f, float alpha);
float iir1_step(iir1_t *f, float x);

#endif
