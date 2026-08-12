#include "firmware/app/scheduler.h"

#define TICKS_1MS   10U
#define TICKS_10MS  100U
#define TICKS_100MS 1000U

void Scheduler_Init(Scheduler *sched, uint32_t now)
{
    sched->last_1ms   = now;
    sched->last_10ms  = now;
    sched->last_100ms = now;
    sched->miss_1ms   = 0UL;
    sched->miss_10ms  = 0UL;
    sched->miss_100ms = 0UL;
}

static int scheduler_take(uint32_t *last, uint32_t *miss,
                          uint32_t now, uint32_t period)
{
    uint32_t elapsed = now - *last;
    if (elapsed < period)
    {
        return 0;
    }

    if (elapsed >= period * 2U)
    {
        (*miss)++;
        *last = now;
    }
    else
    {
        *last += period;
    }
    return 1;
}

int Scheduler_Take1ms(Scheduler *sched, uint32_t now)
{
    return scheduler_take(&sched->last_1ms, &sched->miss_1ms,
                          now, TICKS_1MS);
}

int Scheduler_Take10ms(Scheduler *sched, uint32_t now)
{
    return scheduler_take(&sched->last_10ms, &sched->miss_10ms,
                          now, TICKS_10MS);
}

int Scheduler_Take100ms(Scheduler *sched, uint32_t now)
{
    return scheduler_take(&sched->last_100ms, &sched->miss_100ms,
                          now, TICKS_100MS);
}

void Scheduler_GetDiagnostics(const Scheduler *sched,
                              uint32_t *miss1,
                              uint32_t *miss10,
                              uint32_t *miss100)
{
    *miss1   = sched->miss_1ms;
    *miss10  = sched->miss_10ms;
    *miss100 = sched->miss_100ms;
}
