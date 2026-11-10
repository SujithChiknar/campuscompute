#include "ipc.h"
#include "worker.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

typedef struct { int id, burst, rounds; } SharedJob;
static char memory_name[96], request_name[96], reply_name[96];
static int own_memory, own_request, own_reply;

static int checked(int result, const char *operation)
{
    if (result == -1) {
        perror(operation);
        return 1;
    }
    return 0;
}

int ipc_cleanup(void)
{
    if (own_memory && !checked(shm_unlink(memory_name), "shm_unlink"))
        own_memory = 0;
    if (own_request && !checked(sem_unlink(request_name), "sem_unlink request"))
        own_request = 0;
    if (own_reply && !checked(sem_unlink(reply_name), "sem_unlink reply"))
        own_reply = 0;
    return own_memory || own_request || own_reply;
}

static int wait_request(sem_t *request)
{
    int result;
    do {
        result = sem_wait(request);
    } while (result == -1 && errno == EINTR);
    return checked(result, "sem_wait request");
}

int ipc_run(void)
{
    int fd = -1, failed = 1;
    SharedJob *job = MAP_FAILED;
    sem_t *request = SEM_FAILED, *reply = SEM_FAILED;
    struct timespec stamp;
    if (checked(clock_gettime(CLOCK_REALTIME, &stamp), "clock_gettime"))
        return 1;
    snprintf(memory_name, sizeof memory_name, "/campuscompute_%jd_%jd_%ld",
             (intmax_t)getpid(), (intmax_t)stamp.tv_sec, stamp.tv_nsec);
    snprintf(request_name, sizeof request_name, "%.88s_req", memory_name);
    snprintf(reply_name, sizeof reply_name, "%.88s_rep", memory_name);
    fd = shm_open(memory_name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (checked(fd, "shm_open"))
        goto cleanup;
    own_memory = 1;
    if (checked(ftruncate(fd, sizeof *job), "ftruncate"))
        goto cleanup;
    job = mmap(NULL, sizeof *job, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (job == MAP_FAILED) {
        perror("mmap");
        goto cleanup;
    }
    request = sem_open(request_name, O_CREAT | O_EXCL, 0600, 0);
    if (request == SEM_FAILED) {
        perror("sem_open request");
        goto cleanup;
    }
    own_request = 1;
    reply = sem_open(reply_name, O_CREAT | O_EXCL, 0600, 0);
    if (reply == SEM_FAILED) {
        perror("sem_open reply");
        goto cleanup;
    }
    own_reply = 1;
    /* Inherited handles remain usable after unlink; early unlink also makes
       interruption after fork unable to leave named IPC objects behind. */
    if (ipc_cleanup())
        goto cleanup;
    if (fflush(NULL) == EOF) {
        perror("flush before IPC fork");
        goto cleanup;
    }

    pid_t child = fork();
    if (child == 0) {
        if (wait_request(request))
            _exit(1);
        printf("Child PID %jd received job %d with burst %d after sem_wait(request)\n",
               (intmax_t)getpid(), job->id, job->burst);
        job->rounds = (job->burst + 1) / 2;
        puts("Child wrote result and signals sem_post(reply)");
        int error = checked(sem_post(reply), "sem_post reply");
        if (fflush(stdout) == EOF)
            error = 1;
        error |= checked(sem_close(reply), "child sem_close reply");
        error |= checked(sem_close(request), "child sem_close request");
        error |= checked(munmap(job, sizeof *job), "child munmap");
        error |= checked(close(fd), "child close");
        _exit(error);
    }
    if (child < 0) {
        perror("fork IPC");
        goto cleanup;
    }
    *job = (SharedJob){.id = 17, .burst = 7};
    printf("Parent PID %jd wrote job %d with burst %d and signals sem_post(request)\n",
           (intmax_t)getpid(), job->id, job->burst);
    fflush(stdout);
    if (checked(sem_post(request), "sem_post request")) {
        /* Release a child blocked on a failed handoff. */
        checked(kill(child, SIGKILL), "kill blocked IPC child");
        child_status(child, 0);
        goto cleanup;
    }
    if (child_status(child, 0))
        goto cleanup;
    /* A successful child already posted. A timed wait bounds failure paths. */
    if (checked(clock_gettime(CLOCK_REALTIME, &stamp), "clock_gettime"))
        goto cleanup;
    stamp.tv_sec += 2;
    int result;
    do {
        result = sem_timedwait(reply, &stamp);
    } while (result == -1 && errno == EINTR);
    if (checked(result, "sem_timedwait reply"))
        goto cleanup;
    printf("Parent received %d CPU slices after waiting for reply semaphore\n", job->rounds);
    failed = 0;

cleanup:
    if (reply != SEM_FAILED)
        failed |= checked(sem_close(reply), "sem_close reply");
    if (request != SEM_FAILED)
        failed |= checked(sem_close(request), "sem_close request");
    if (job != MAP_FAILED)
        failed |= checked(munmap(job, sizeof *job), "munmap");
    if (fd >= 0)
        failed |= checked(close(fd), "close shared memory");
    failed |= ipc_cleanup();
    if (!failed)
        puts("Shared-memory cleanup verified\nSemaphore cleanup verified");
    return failed;
}
