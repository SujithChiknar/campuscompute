#include "worker.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

int child_status(pid_t child, int expected)
{
    int status;
    pid_t result;
    do {
        result = waitpid(child, &status, 0);
    } while (result < 0 && errno == EINTR);
    if (result < 0) {
        perror("waitpid");
        return 1;
    }
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        printf("Child PID %jd reaped, exit status %d\n", (intmax_t)child, code);
        if (code == expected)
            return 0;
        fprintf(stderr, "Expected child exit status %d\n", expected);
    } else if (WIFSIGNALED(status)) {
        fprintf(stderr, "Child PID %jd reaped after signal %d\n",
                (intmax_t)child, WTERMSIG(status));
    } else {
        fputs("Unexpected child status\n", stderr);
    }
    return 1;
}

int worker_run(int fail)
{
    enum { CONTROLLED_EXIT = 42 };
    if (fflush(NULL) == EOF) {
        perror("flush before fork");
        return 1;
    }
    pid_t child = fork();
    if (child < 0) {
        perror("fork worker");
        return 1;
    }
    if (child == 0) {
        if (fail) {
            puts("CONTROLLED SIMULATED FAILURE (intentional exit 42)");
        } else {
            int sum = 0;
            for (int i = 1; i <= 10; ++i)
                sum += i * i;
            printf("Worker sum of squares from 1 to 10 = %d\n", sum);
        }
        if (fflush(stdout) == EOF)
            _exit(1);
        _exit(fail ? CONTROLLED_EXIT : 0);
    }
    if (child_status(child, fail ? CONTROLLED_EXIT : 0))
        return 1;
    puts(fail ? "Parent detected controlled failure" : "Parent confirmed normal completion");
    return 0;
}
