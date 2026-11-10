#include "logger.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

enum { CAPACITY = 3, PRODUCERS = 2, EVENTS_PER_PRODUCER = 5 };
typedef struct {
    time_t timestamp;
    const char *type, *description;
    int producer;
} Event;
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t not_empty, not_full;
    Event events[CAPACITY];
    size_t head, size, produced, consumed, completed;
    int closed, io_error;
    FILE *file;
} Queue;
typedef struct { Queue *queue; int id; } Producer;

static int thread_error(int code, const char *operation)
{
    if (code)
        fprintf(stderr, "%s: %s\n", operation, strerror(code));
    return code != 0;
}

static void require_sync(int code, const char *operation)
{
    /* A failed operation on an initialized synchronization object leaves
       no reliable way to continue coordinating the live threads. */
    if (thread_error(code, operation))
        abort();
}

static void *produce(void *arg)
{
    static const char *const types[] = {"SUBMIT", "READY", "DISPATCH", "RUN", "COMPLETE"};
    static const char *const descriptions[] = {
        "Lab job submitted", "Job admitted to ready queue", "CPU assigned",
        "CPU burst executing", "Job completed"
    };
    Producer *producer = arg;
    Queue *q = producer->queue;
    for (int i = 0; i < EVENTS_PER_PRODUCER; ++i) {
        Event e = {time(NULL), types[i], descriptions[i], producer->id};
        require_sync(pthread_mutex_lock(&q->mutex), "producer lock");
        while (q->size == CAPACITY)
            require_sync(pthread_cond_wait(&q->not_full, &q->mutex), "wait for space");
        q->events[(q->head + q->size) % CAPACITY] = e;
        ++q->size;
        ++q->produced;
        printf("enqueue producer=%d event=%s queue=%zu\n", e.producer, e.type, q->size);
        require_sync(pthread_cond_signal(&q->not_empty), "signal event");
        require_sync(pthread_mutex_unlock(&q->mutex), "producer unlock");
    }
    require_sync(pthread_mutex_lock(&q->mutex), "completion lock");
    ++q->completed;
    require_sync(pthread_mutex_unlock(&q->mutex), "completion unlock");
    return NULL;
}

static void *consume(void *arg)
{
    Queue *q = arg;
    for (;;) {
        require_sync(pthread_mutex_lock(&q->mutex), "consumer lock");
        while (!q->size && !q->closed)
            require_sync(pthread_cond_wait(&q->not_empty, &q->mutex), "wait for event");
        if (!q->size) {
            require_sync(pthread_mutex_unlock(&q->mutex), "consumer unlock");
            return NULL;
        }
        Event e = q->events[q->head];
        q->head = (q->head + 1) % CAPACITY;
        --q->size;
        ++q->consumed;
        printf("dequeue producer=%d event=%s queue=%zu\n", e.producer, e.type, q->size);
        require_sync(pthread_cond_signal(&q->not_full), "signal space");
        require_sync(pthread_mutex_unlock(&q->mutex), "consumer unlock");

        struct tm utc;
        char stamp[32];
        if (!gmtime_r(&e.timestamp, &utc) ||
            !strftime(stamp, sizeof stamp, "%Y-%m-%dT%H:%M:%SZ", &utc)) {
            fputs("logger: timestamp formatting failed\n", stderr);
            q->io_error = 1;
        } else if (fprintf(q->file, "%s %-8s producer=%d %s\n",
                           stamp, e.type, e.producer, e.description) < 0) {
            if (!q->io_error)
                perror("write log");
            q->io_error = 1;
        }
    }
}

int logger_run(void)
{
    Queue q = {0};
    pthread_t consumer, producers[PRODUCERS];
    Producer args[PRODUCERS];
    size_t started = 0;
    int failed = 1;

    if (mkdir("logs", 0755) && errno != EEXIST) {
        perror("mkdir logs");
        return 1;
    }
    q.file = fopen("logs/campuscompute.log", "a");
    if (!q.file) {
        perror("open activity log");
        return 1;
    }
    if (thread_error(pthread_mutex_init(&q.mutex, NULL), "initialize mutex"))
        goto close_file;
    if (thread_error(pthread_cond_init(&q.not_empty, NULL), "initialize not_empty"))
        goto destroy_mutex;
    if (thread_error(pthread_cond_init(&q.not_full, NULL), "initialize not_full"))
        goto destroy_empty;
    if (thread_error(pthread_create(&consumer, NULL, consume, &q), "create consumer"))
        goto destroy_full;
    for (; started < PRODUCERS; ++started) {
        args[started] = (Producer){&q, (int)started + 1};
        if (thread_error(pthread_create(&producers[started], NULL, produce, &args[started]),
                         "create producer"))
            break;
    }
    for (size_t i = 0; i < started; ++i)
        require_sync(pthread_join(producers[i], NULL), "join producer");

    /* All started producers are gone. Wake the consumer to drain and exit. */
    require_sync(pthread_mutex_lock(&q.mutex), "close queue lock");
    q.closed = 1;
    require_sync(pthread_cond_signal(&q.not_empty), "signal queue close");
    require_sync(pthread_mutex_unlock(&q.mutex), "close queue unlock");
    require_sync(pthread_join(consumer, NULL), "join consumer");
    printf("Produced=%zu Consumed=%zu Final queue=%zu Completed producers=%zu\n",
           q.produced, q.consumed, q.size, q.completed);
    puts("All started threads joined successfully");
    failed = started != PRODUCERS || q.completed != PRODUCERS ||
        q.produced != q.consumed || q.size != 0 || q.io_error;
destroy_full:
    failed |= thread_error(pthread_cond_destroy(&q.not_full), "destroy not_full");
destroy_empty:
    failed |= thread_error(pthread_cond_destroy(&q.not_empty), "destroy not_empty");
destroy_mutex:
    failed |= thread_error(pthread_mutex_destroy(&q.mutex), "destroy mutex");
close_file:
    if (fclose(q.file)) {
        perror("close activity log");
        failed = 1;
    }
    return failed;
}
