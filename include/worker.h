#ifndef WORKER_H
#define WORKER_H
#include <sys/types.h>
int worker_run(int fail);
int child_status(pid_t child, int expected);
#endif
