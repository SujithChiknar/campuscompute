#ifndef BANKER_H
#define BANKER_H
/* expected is -1 for either outcome, 0 for unsafe, 1 for safe. */
int banker_run(const char *path, int expected);
#endif
