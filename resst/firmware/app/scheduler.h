#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

typedef struct
{
    uint32_t last_1ms;
    uint32_t last_10ms;
    uint32_t last_100ms;
    uint32_t miss_1ms;
    uint32_t miss_10ms;
    uint32_t miss_100ms;
} Scheduler;

void Scheduler_Init(Scheduler *sched, uint32_t now);

int Scheduler_Take1ms(Scheduler *sched, uint32_t now);
int Scheduler_Take10ms(Scheduler *sched, uint32_t now);
int Scheduler_Take100ms(Scheduler *sched, uint32_t now);

void Scheduler_GetDiagnostics(const Scheduler *sched,
                              uint32_t *miss1,
                              uint32_t *miss10,
                              uint32_t *miss100);

#endif
