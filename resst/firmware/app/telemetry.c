/* Created by Siquanning */
#include "firmware/app/telemetry.h"
#include <string.h>

void Telemetry_Init(Telemetry *t)
{
    if (t == ((Telemetry *)0)) return;

    memset(t, 0, sizeof(Telemetry));
}

void Telemetry_WriteFastSnapshot(Telemetry *t,
                                 uint16_t state,
                                 const uint16_t adc_raw[2],
                                 const uint16_t cmpa[3],
                                 const uint16_t cmpb[3],
                                 uint16_t output_valid,
                                 uint16_t trip_flags,
                                 uint16_t fault_code,
                                 uint16_t step_count)
{
    TelemetryFastSnapshot *buf;
    uint16_t idx;
    uint16_t i;

    if (t == ((Telemetry *)0)) return;

    idx = t->active_idx;
    buf = &t->buffer[idx];

    buf->state        = state;
    buf->adc_raw[0]   = adc_raw[0];
    buf->adc_raw[1]   = adc_raw[1];
    for (i = 0U; i < 3U; i++)
    {
        buf->cmpa[i] = cmpa[i];
        buf->cmpb[i] = cmpb[i];
    }
    buf->output_valid  = output_valid;
    buf->trip_flags    = trip_flags;
    buf->fault_code    = fault_code;
    buf->step_count    = step_count;

    buf->version++;
    t->write_count++;

    if (idx == t->read_idx)
    {
        t->overrun_count++;
    }
}

int Telemetry_ReadSnapshot(Telemetry *t,
                           TelemetryFastSnapshot *out)
{
    uint16_t idx;
    uint16_t version_before;
    uint16_t version_after;

    if (t   == ((Telemetry *)0)) return 0;
    if (out == ((TelemetryFastSnapshot *)0)) return 0;

    idx = t->active_idx;
    t->read_idx = idx;

    version_before = t->buffer[idx].version;

    memcpy(out, &t->buffer[idx], sizeof(TelemetryFastSnapshot));

    version_after = t->buffer[idx].version;

    if (version_before != version_after)
    {
        t->overrun_count++;
        return 0;
    }

    return 1;
}
