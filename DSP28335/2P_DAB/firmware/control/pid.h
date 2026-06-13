#ifndef CONTROL_PID_H
#define CONTROL_PID_H

// Parallel PID controller.  All gains and states are float, directly
// settable via CCS debugger for online tuning.

typedef struct {
    // Tunable gains (writable in debugger at any time)
    float Kp;          // proportional gain
    float Ki;          // integral gain
    float Kd;          // derivative gain

    // Output limits
    float out_min;
    float out_max;

    // Internal state (readable in debugger)
    float dt;          // sample time in seconds (0.001 = 1ms)
    float integral;    // accumulated integral term
    float prev_error;  // previous error, for derivative
    float output;      // last output value
} pid_t;

// Initialize a PID with given gains, dt in seconds, and output limits.
void pid_init(pid_t *pid, float Kp, float Ki, float Kd,
              float dt, float out_min, float out_max);

// Compute one PID step.  Returns output clamped to [out_min, out_max].
// setpoint: desired V2,  feedback: measured V2.
float pid_step(pid_t *pid, float setpoint, float feedback);

// Reset integral and previous error (e.g. on state transition).
void pid_reset(pid_t *pid);

#endif
