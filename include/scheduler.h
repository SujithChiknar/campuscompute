#ifndef SCHEDULER_H
#define SCHEDULER_H
#include "jobs.h"

typedef enum { FCFS, RR, PRIORITY } Policy;
void schedule(const Job jobs[], size_t count, Policy policy, int quantum);
void compare(const Job jobs[], size_t count);
#endif
