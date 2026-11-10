#include "resource.h"
#include "worker.h"
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

static char temporary[] = "/tmp/campuscompute-limit-XXXXXX";
static int owns_file;

int resource_cleanup(void)
{
    if (owns_file && unlink(temporary)) {
        perror("unlink limit experiment file");
        return 1;
    }
    owns_file = 0;
    return 0;
}

static void show_limit(FILE *report, const char *label, rlim_t value)
{
    if (value == RLIM_INFINITY)
        fprintf(report, "%s=unlimited\n", label);
    else
        fprintf(report, "%s=%ju bytes\n", label, (uintmax_t)value);
}

static int experiment(int fd, FILE *report)
{
    struct rlimit limit;
    if (getrlimit(RLIMIT_FSIZE, &limit)) {
        perror("getrlimit");
        return 1;
    }
    show_limit(report, "Original soft limit", limit.rlim_cur);
    show_limit(report, "Original hard limit", limit.rlim_max);
    if (limit.rlim_cur > 1024)
        limit.rlim_cur = 1024;
    struct sigaction action = {.sa_handler = SIG_IGN};
    if (sigemptyset(&action.sa_mask) || sigaction(SIGXFSZ, &action, NULL) ||
        setrlimit(RLIMIT_FSIZE, &limit) || getrlimit(RLIMIT_FSIZE, &limit)) {
        perror("configure child file size limit");
        return 1;
    }
    show_limit(report, "Applied soft limit", limit.rlim_cur);
    fputs("SIGXFSZ ignored in child so write errors can be observed\n", report);
    char data[2048] = {0};
    size_t total = 0;
    while (total < sizeof data) {
        size_t requested = sizeof data - total;
        ssize_t written = write(fd, data + total, requested);
        if (written < 0) {
            int error = errno;
            if (error == EINTR)
                continue;
            fprintf(report, "write requested %zu bytes, returned %zd, errno=%d (%s)\n",
                   requested, written, error, strerror(error));
            fprintf(report, "Total bytes written: %zu\n", total);
            return error != EFBIG || total > limit.rlim_cur;
        }
        fprintf(report, "write requested %zu bytes, returned %zd\n", requested, written);
        if (!written)
            break;
        total += (size_t)written;
    }
    fputs("Expected the file size limit to stop the write\n", stderr);
    return 1;
}

int resource_run(void)
{
    if (resource_cleanup())
        return 1;
    strcpy(temporary, "/tmp/campuscompute-limit-XXXXXX");
    int fd = mkstemp(temporary);
    if (fd < 0) {
        perror("mkstemp");
        return 1;
    }
    owns_file = 1;
    /* The open descriptor survives unlink and is inherited by the child. */
    int failed = resource_cleanup();
    int channel[2];
    if (failed || fflush(NULL) == EOF || pipe(channel)) {
        if (!failed)
            perror("prepare limit report pipe");
        if (close(fd))
            perror("close limit file");
        return 1;
    }
    /* The child's limit also applies to redirected stdout. Relay its
       report through a pipe so only the parent writes to stdout. */
    pid_t child = fork();
    if (child == 0) {
        if (close(channel[0]))
            _exit(1);
        FILE *report = fdopen(channel[1], "w");
        if (!report) {
            perror("open limit report");
            _exit(1);
        }
        int result = experiment(fd, report);
        if (close(fd)) {
            perror("finish child limit experiment");
            result = 1;
        }
        if (fclose(report))
            result = 1;
        _exit(result);
    }
    if (child < 0)
        perror("fork limit");
    if (close(channel[1])) {
        perror("close limit report writer");
        failed = 1;
    }
    if (close(fd)) {
        perror("close limit file");
        failed = 1;
    }
    if (child < 0)
        failed = 1;
    else {
        char buffer[512];
        for (;;) {
            ssize_t received = read(channel[0], buffer, sizeof buffer);
            if (received < 0 && errno == EINTR)
                continue;
            if (received <= 0) {
                if (received < 0) {
                    perror("read limit report");
                    failed = 1;
                }
                break;
            }
            if (fwrite(buffer, 1, (size_t)received, stdout) != (size_t)received)
                failed = 1;
        }
        failed |= child_status(child, 0);
    }
    if (close(channel[0])) {
        perror("close limit report reader");
        failed = 1;
    }
    failed |= resource_cleanup();
    if (!failed)
        puts("Temporary file removed");
    return failed;
}
