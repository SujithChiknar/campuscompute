#include "banker.h"
#include "ipc.h"
#include "jobs.h"
#include "logger.h"
#include "resource.h"
#include "scheduler.h"
#include "worker.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

static void help(void)
{
    puts("CampusCompute | UE25CS242A Operating Systems\n"
         "  campuscompute help\n"
         "  campuscompute jobs [CSV]\n"
         "  campuscompute schedule fcfs|rr|priority [CSV|-] [QUANTUM]\n"
         "  campuscompute compare [CSV]\n"
         "  campuscompute logger | ipc | limit | demo | cleanup\n"
         "  campuscompute banker FILE\n"
         "  campuscompute worker normal|fail\n"
         "Omit CSV or use - to select the built-in sample.\n"
         "Only rr accepts QUANTUM (positive integer, default 2).\n"
         "Run from the project directory to use bundled demo fixtures and logs.");
}

static int cleanup(void)
{
    int failed = ipc_cleanup();
    failed |= resource_cleanup();
    if (!failed)
        puts("Cleanup verified: no pending IPC names or temporary files owned by this process.");
    return failed;
}

static int demo(void)
{
    Job jobs[MAX_JOBS];
    size_t count;
    int load = jobs_load("tests/jobs.csv", jobs, &count) != 0;
    if (!load) {
        jobs_print(jobs, count);
        for (Policy policy = FCFS; policy <= PRIORITY; ++policy)
            schedule(jobs, count, policy, 2);
        puts("Scheduling comparison");
        compare(jobs, count);
    }

    const char *const labels[] = {"Job loading", "Scheduling and comparison",
        "Logger", "Shared memory IPC", "Banker safe fixture", "Banker unsafe fixture",
        "Normal worker", "Controlled worker failure", "Resource limit", "Cleanup"};
    int results[sizeof labels / sizeof *labels];
    results[0] = load;
    results[1] = load;
    results[2] = logger_run();
    results[3] = ipc_run();
    results[4] = banker_run("tests/banker_safe.txt", 1);
    results[5] = banker_run("tests/banker_unsafe.txt", 0);
    results[6] = worker_run(0);
    results[7] = worker_run(1);
    results[8] = resource_run();
    results[9] = cleanup();

    int failed = 0;
    puts("Demo summary");
    for (size_t i = 0; i < sizeof labels / sizeof *labels; ++i) {
        printf("  %-26s %s\n", labels[i], results[i] ? "FAIL" : "PASS");
        failed |= results[i] != 0;
    }
    return failed;
}

static int usage_error(const char *message)
{
    fprintf(stderr, "%s. Run ./campuscompute help for usage.\n", message);
    return 2;
}

static const char *sample_or_file(const char *arg)
{
    return arg && strcmp(arg, "-") ? arg : NULL;
}

int main(int argc, char **argv)
{
    if (argc < 2)
        return usage_error("Missing command");
    const char *command = argv[1];
    if (!strcmp(command, "jobs") || !strcmp(command, "compare") ||
        !strcmp(command, "schedule")) {
        Job jobs[MAX_JOBS];
        size_t count;
        const char *path;
        int quantum = 2;
        Policy policy = FCFS;
        if (!strcmp(command, "schedule")) {
            if (argc < 3 || argc > 5)
                return usage_error("Expected schedule POLICY [CSV|-] [QUANTUM]");
            if (!strcmp(argv[2], "rr"))
                policy = RR;
            else if (!strcmp(argv[2], "priority"))
                policy = PRIORITY;
            else if (strcmp(argv[2], "fcfs"))
                return usage_error("Unknown scheduling policy");
            if (argc == 5 && (policy != RR || parse_int(argv[4], 1, INT_MAX, &quantum)))
                return usage_error("Quantum must be a positive integer and is only valid for rr");
            path = argc >= 4 ? sample_or_file(argv[3]) : NULL;
        } else {
            if (argc > 3)
                return usage_error("Expected at most one CSV file");
            path = sample_or_file(argv[2]);
        }
        if (jobs_load(path, jobs, &count))
            return 1;
        if (!strcmp(command, "jobs"))
            jobs_print(jobs, count);
        else if (!strcmp(command, "compare"))
            compare(jobs, count);
        else
            schedule(jobs, count, policy, quantum);
        return 0;
    }

    if (!strcmp(command, "banker"))
        return argc == 3 ? banker_run(argv[2], -1) : usage_error("Expected banker FILE");
    if (!strcmp(command, "worker")) {
        if (argc != 3 || (strcmp(argv[2], "normal") && strcmp(argv[2], "fail")))
            return usage_error("Expected worker normal|fail");
        return worker_run(!strcmp(argv[2], "fail"));
    }
    if (argc != 2)
        return usage_error("Unexpected arguments");
    if (!strcmp(command, "help")) {
        help();
        return 0;
    }
    if (!strcmp(command, "logger"))
        return logger_run();
    if (!strcmp(command, "ipc"))
        return ipc_run();
    if (!strcmp(command, "limit"))
        return resource_run();
    if (!strcmp(command, "demo"))
        return demo();
    if (!strcmp(command, "cleanup"))
        return cleanup();
    return usage_error("Unknown command");
}
