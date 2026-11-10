#include "scheduler.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    double waiting, turnaround, utilization, throughput;
    int64_t elapsed;
} Metrics;

static const char *const names[] = {"FCFS", "Round Robin", "Priority"};

static void segment(int64_t start, int64_t end, int id, int show)
{
    if (!show)
        return;
    printf("[%" PRId64 ",%" PRId64 ") ", start, end);
    if (id)
        printf("J%d\n", id);
    else
        puts("IDLE");
}

static Metrics simulate(const Job input[], size_t count, Policy policy,
                        int quantum, int show)
{
    Job jobs[MAX_JOBS];
    size_t order[MAX_JOBS], queue[MAX_JOBS];
    memcpy(jobs, input, count * sizeof *jobs);
    for (size_t i = 0; i < count; ++i) {
        size_t j = i;
        while (j > 0 && jobs[order[j - 1]].arrival > jobs[i].arrival) {
            order[j] = order[j - 1];
            --j;
        }
        order[j] = i;
    }

    size_t next = 0, head = 0, ready = 0, done = 0;
    int64_t now = 0, busy = 0;
    Metrics metrics = {0};
    if (show)
        printf("%s timeline (logical time)\n", names[policy]);
    while (done < count) {
        while (next < count && jobs[order[next]].arrival <= now) {
            size_t index = order[next++];
            jobs[index].status = READY;
            if (policy == RR)
                queue[(head + ready++) % MAX_JOBS] = index;
        }

        size_t chosen = count;
        if (policy == RR && ready) {
            chosen = queue[head];
            head = (head + 1) % MAX_JOBS;
            --ready;
        } else if (policy != RR) {
            for (size_t i = 0; i < next; ++i) {
                size_t index = order[i];
                if (jobs[index].status != READY)
                    continue;
                if (chosen == count || (policy == PRIORITY &&
                    jobs[index].priority < jobs[chosen].priority))
                    chosen = index;
            }
        }
        if (chosen == count) {
            int64_t arrival = jobs[order[next]].arrival;
            segment(now, arrival, 0, show);
            now = arrival;
            continue;
        }

        Job *job = &jobs[chosen];
        job->status = RUNNING;
        int slice = job->remaining;
        if (policy == RR && slice > quantum)
            slice = quantum;
        segment(now, now + slice, job->id, show);
        now += slice;
        busy += slice;
        job->remaining -= slice;
        if (job->remaining) {
            /* Admit arrivals through the slice boundary before requeuing. */
            while (next < count && jobs[order[next]].arrival <= now) {
                size_t index = order[next++];
                jobs[index].status = READY;
                queue[(head + ready++) % MAX_JOBS] = index;
            }
            queue[(head + ready++) % MAX_JOBS] = chosen;
            job->status = READY;
        } else {
            job->status = COMPLETED;
            job->completion = now;
            job->turnaround = now - job->arrival;
            job->waiting = job->turnaround - job->burst;
            metrics.waiting += job->waiting;
            metrics.turnaround += job->turnaround;
            ++done;
        }
    }
    if (show) {
        puts("ID CT TAT WT Status");
        for (size_t i = 0; i < count; ++i)
            printf("%d %" PRId64 " %" PRId64 " %" PRId64 " COMPLETED\n",
                   jobs[i].id, jobs[i].completion, jobs[i].turnaround, jobs[i].waiting);
    }
    metrics.waiting /= count;
    metrics.turnaround /= count;
    metrics.elapsed = now;
    metrics.utilization = 100.0 * busy / now;
    metrics.throughput = (double)count / now;
    return metrics;
}

static void print_metrics(Policy policy, Metrics m)
{
    printf("%-12s %8.2f %8.2f %12" PRId64 " %9.2f%% %11.4f\n",
           names[policy], m.waiting, m.turnaround, m.elapsed, m.utilization,
           m.throughput);
}

static void metrics_header(void)
{
    puts("Policy         Avg WT  Avg TAT      Elapsed       CPU  Jobs/unit");
}

void schedule(const Job jobs[], size_t count, Policy policy, int quantum)
{
    Metrics m = simulate(jobs, count, policy, quantum, 1);
    metrics_header();
    print_metrics(policy, m);
}

void compare(const Job jobs[], size_t count)
{
    metrics_header();
    for (Policy policy = FCFS; policy <= PRIORITY; ++policy)
        print_metrics(policy, simulate(jobs, count, policy, 2, 0));
}
