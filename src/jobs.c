#include "jobs.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int parse_int(const char *text, int min, int max, int *value)
{
    char *end;
    errno = 0;
    long number = strtol(text, &end, 10);
    if (errno || end == text || number < min || number > max)
        return -1;
    while (isspace((unsigned char)*end))
        ++end;
    if (*end)
        return -1;
    *value = (int)number;
    return 0;
}

int jobs_load(const char *path, Job jobs[], size_t *count)
{
    static const int sample[][4] = {
        {1, 2, 5, 2}, {2, 2, 3, 1}, {3, 4, 2, 1}, {4, 15, 1, 0}
    };
    *count = 0;
    if (!path) {
        for (size_t i = 0; i < sizeof sample / sizeof sample[0]; ++i)
            jobs[(*count)++] = (Job){.id = sample[i][0], .arrival = sample[i][1],
                .burst = sample[i][2], .priority = sample[i][3],
                .remaining = sample[i][2], .status = NEW};
        return 0;
    }

    FILE *file = fopen(path, "r");
    if (!file) {
        perror(path);
        return -1;
    }

    char *line = NULL;
    size_t capacity = 0, row = 0;
    const char *error = NULL;
    ssize_t length;
    while ((length = getline(&line, &capacity, file)) >= 0) {
        ++row;
        if (memchr(line, '\0', (size_t)length)) {
            error = "embedded NUL byte";
            break;
        }
        while (length > 0 && (line[length - 1] == '\r' || line[length - 1] == '\n'))
            line[--length] = '\0';
        if (row == 1 && !strcmp(line, "id,arrival,burst,priority"))
            continue;
        if (*count == MAX_JOBS) {
            error = "maximum job count exceeded";
            break;
        }
        int values[4];
        const int mins[] = {1, 0, 1, 0};
        const int maxes[] = {INT_MAX, INT_MAX, MAX_BURST, MAX_PRIORITY};
        char *field = line;
        for (size_t i = 0; i < 4; ++i) {
            char *comma = strchr(field, ',');
            if ((comma != NULL) != (i < 3)) {
                error = "expected four CSV fields";
                break;
            }
            if (comma)
                *comma = '\0';
            if (parse_int(field, mins[i], maxes[i], &values[i])) {
                error = "invalid job ID, arrival, burst, or priority";
                break;
            }
            if (comma)
                field = comma + 1;
        }
        if (error)
            break;
        for (size_t i = 0; i < *count; ++i)
            if (jobs[i].id == values[0])
                error = "duplicate job ID";
        if (error)
            break;
        jobs[(*count)++] = (Job){.id = values[0], .arrival = values[1],
            .burst = values[2], .priority = values[3], .remaining = values[2],
            .status = NEW};
    }
    if (!error && !feof(file))
        error = "error reading jobs";
    if (!error && !*count)
        error = "no jobs supplied";
    free(line);
    if (fclose(file) && !error)
        error = "error closing jobs file";
    if (error) {
        fprintf(stderr, "%s (line %zu): %s\n", path, row, error);
        return -1;
    }
    return 0;
}

void jobs_print(const Job jobs[], size_t count)
{
    puts("ID Arrival Burst Priority Remaining Status");
    for (size_t i = 0; i < count; ++i)
        printf("%d %d %d %d %d NEW\n", jobs[i].id, jobs[i].arrival,
               jobs[i].burst, jobs[i].priority, jobs[i].remaining);
}
