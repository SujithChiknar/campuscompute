#ifndef JOBS_H
#define JOBS_H
#include <stddef.h>
#include <stdint.h>

#define MAX_JOBS 128
#define MAX_BURST 10000
#define MAX_PRIORITY 99

typedef enum { NEW, READY, RUNNING, COMPLETED } Status;
typedef struct {
    int id, arrival, burst, priority, remaining;
    int64_t completion, turnaround, waiting;
    Status status;
} Job;

int parse_int(const char *text, int min, int max, int *value);
int jobs_load(const char *path, Job jobs[], size_t *count);
void jobs_print(const Job jobs[], size_t count);
#endif
