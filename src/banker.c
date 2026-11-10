#include "banker.h"
#include "jobs.h"
#include <limits.h>
#include <stdio.h>

enum { MAX_PROCESSES = 32, MAX_RESOURCES = 16 };
typedef struct {
    int processes, resources;
    int available[MAX_RESOURCES];
    int allocation[MAX_PROCESSES][MAX_RESOURCES];
    int maximum[MAX_PROCESSES][MAX_RESOURCES];
    int need[MAX_PROCESSES][MAX_RESOURCES];
} State;

static int read_number(FILE *file, int min, int max, int *out)
{
    /* Reject overlong tokens rather than interpreting them as two numbers. */
    char token[64];
    if (fscanf(file, "%63s", token) != 1)
        return 1;
    size_t length = 0;
    while (token[length])
        ++length;
    return length == sizeof token - 1 || parse_int(token, min, max, out);
}

static int read_state(FILE *file, State *state)
{
    if (read_number(file, 1, MAX_PROCESSES, &state->processes) ||
        read_number(file, 1, MAX_RESOURCES, &state->resources))
        return 1;
    int total[MAX_RESOURCES];
    for (int j = 0; j < state->resources; ++j) {
        if (read_number(file, 0, INT_MAX, &state->available[j]))
            return 1;
        total[j] = state->available[j];
    }
    for (int i = 0; i < state->processes; ++i)
        for (int j = 0; j < state->resources; ++j) {
            int *allocation = &state->allocation[i][j];
            if (read_number(file, 0, INT_MAX - total[j], allocation))
                return 1;
            total[j] += *allocation;
        }
    for (int i = 0; i < state->processes; ++i)
        for (int j = 0; j < state->resources; ++j) {
            int allocation = state->allocation[i][j];
            if (read_number(file, allocation, total[j], &state->maximum[i][j]))
                return 1;
            state->need[i][j] = state->maximum[i][j] - allocation;
        }
    char trailing;
    return fscanf(file, " %c", &trailing) != EOF || ferror(file);
}

static void vector(const char *label, const int values[], int count)
{
    printf("%s [", label);
    for (int j = 0; j < count; ++j)
        printf("%s%d", j ? " " : "", values[j]);
    puts("]");
}

static void matrix(const char *label, int values[][MAX_RESOURCES], const State *state)
{
    puts(label);
    for (int i = 0; i < state->processes; ++i) {
        printf("P%d", i);
        vector("", values[i], state->resources);
    }
}

int banker_run(const char *path, int expected)
{
    FILE *file = fopen(path, "r");
    if (!file) {
        perror(path);
        return 1;
    }
    State state;
    int invalid = read_state(file, &state);
    if (fclose(file)) {
        perror(path);
        return 1;
    }
    if (invalid) {
        fprintf(stderr, "%s: invalid Banker dimensions, counts, claims, or resource totals\n", path);
        return 1;
    }

    vector("Available", state.available, state.resources);
    matrix("Allocation", state.allocation, &state);
    matrix("Maximum", state.maximum, &state);
    matrix("Need", state.need, &state);

    int work[MAX_RESOURCES], finish[MAX_PROCESSES] = {0}, sequence[MAX_PROCESSES];
    for (int j = 0; j < state.resources; ++j)
        work[j] = state.available[j];
    vector("Initial Work", work, state.resources);
    int done = 0, previous;
    do {
        previous = done;
        for (int i = 0; i < state.processes; ++i) {
            if (finish[i])
                continue;
            int j = 0;
            while (j < state.resources && state.need[i][j] <= work[j])
                ++j;
            if (j != state.resources)
                continue;
            for (j = 0; j < state.resources; ++j)
                work[j] += state.allocation[i][j];
            finish[i] = 1;
            sequence[done++] = i;
            printf("After P%d: ", i);
            vector("Work", work, state.resources);
        }
    } while (done < state.processes && done > previous);

    int safe = done == state.processes;
    if (safe) {
        puts("SAFE STATE");
        printf("Safe sequence: ");
        for (int i = 0; i < done; ++i)
            printf("%sP%d", i ? " -> " : "", sequence[i]);
        putchar('\n');
    } else {
        puts("UNSAFE STATE");
        for (int i = 0; i < state.processes; ++i)
            if (!finish[i])
                printf("P%d cannot finish because its remaining need exceeds Work in at least one resource\n", i);
        puts("Unsafe means maximum claims cannot be guaranteed; current deadlock is undetermined.");
    }
    if (expected >= 0 && expected != safe) {
        fputs("Banker fixture produced an unexpected safety classification\n", stderr);
        return 1;
    }
    return 0;
}
