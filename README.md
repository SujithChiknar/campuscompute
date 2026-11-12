# CampusCompute

**Intelligent Job Scheduling and Resource Management**  
UE25CS242A — Operating Systems

CampusCompute simulates jobs submitted to a university computing lab. CPU scheduling runs as a deterministic simulation on a logical clock. The logger uses real POSIX threads, while the IPC, worker and file-size experiments spawn real Linux child processes.

## Build on Ubuntu or WSL Ubuntu

On Windows, if Ubuntu isn't installed yet, open an administrator PowerShell, run `wsl --install -d Ubuntu`, and finish Ubuntu's initial setup. Then, in the Ubuntu terminal, move into this project directory. A Windows checkout under `C:\Users` can be reached at `/mnt/c/Users`.

```bash
sudo apt update
sudo apt install build-essential zip unzip
make clean
make
make test
```

The project relies on GCC, Make, Bash, and the standard Ubuntu command-line tools. The ZIP tools are only needed for packaging. `make rebuild` does a clean build, and `make clean` deletes the executable, object files and dependency files.

## Commands

Run all of these from the project directory.

```bash
./campuscompute help
./campuscompute jobs tests/jobs.csv
./campuscompute schedule fcfs tests/jobs.csv
./campuscompute schedule rr tests/jobs.csv 2
./campuscompute schedule priority tests/jobs.csv
./campuscompute compare tests/jobs.csv
./campuscompute logger
./campuscompute ipc
./campuscompute banker tests/banker_safe.txt
./campuscompute banker tests/banker_unsafe.txt
./campuscompute worker normal
./campuscompute worker fail
./campuscompute limit
./campuscompute demo
./campuscompute cleanup
```

For jobs, schedule and compare, leave out the CSV or pass `-` to use the repeatable built-in sample. For instance, `./campuscompute schedule rr - 3` runs the sample with a quantum of 3. An exit status of 0 means the run succeeded, which includes a demonstrated unsafe Banker state or a worker that intentionally exits with 42. Malformed data or an operational failure returns 1, and CLI usage errors return 2.

## Input and scheduling

The job CSV may begin with the exact header `id,arrival,burst,priority`, followed by one unquoted record per line. The loader accepts 1–128 jobs with unique positive integer IDs, nonnegative integer arrivals, bursts from 1 to 10,000, and priorities from 0 to 99. IDs and arrivals must fit in a signed C int. Logical times are 64-bit integers, so later completions can exceed the arrival range.

- FCFS picks the job with the earliest arrival, with input order breaking arrival ties. Every record has a unique input position, so an ID tie-break is never required.
- Round Robin uses a FIFO ready queue and a positive integer quantum (default 2). After each slice, newly arrived jobs join the tail in arrival and input order, including any that arrive exactly at the slice boundary. The preempted job is then placed behind them.
- Non-preemptive priority selects the ready job with the lowest priority number, breaking ties by arrival and then input order. A running job holds the CPU until it completes.

A timeline row `[a,b)` spans logical times from a (inclusive) to b (exclusive). Idle intervals are shown explicitly. CT is the time a job finishes, TAT is CT minus arrival, and WT is TAT minus burst. Elapsed time begins at zero and ends at the last completion. Utilization is busy time divided by elapsed time, and throughput is completed jobs divided by elapsed time. Compare runs every policy on its own private copy of the same jobs, using RR quantum 2.

## Logger, IPC and resource safety

The logger appends to `logs/campuscompute.log`. Two producer threads each emit five lab-job lifecycle events. A mutex guards the three-slot circular queue and its counters. Producers wait on not-full and the consumer waits on not-empty, rechecking the predicate after every wakeup. The consumer releases the mutex before writing each event to the log file. Main joins the producers, closes the queue and wakes the consumer, which drains any remaining events before main joins it.

IPC creates exclusive shared-memory and named semaphore objects and opens their handles before forking. Once the handles exist, the names are removed. The inherited mapping and semaphore handles keep the objects alive until both processes release them. The parent publishes the job and posts the request semaphore. The child waits for that request, computes the number of two-unit CPU slices, and posts the reply. The parent reaps the child and waits on the reply semaphore before reading the result.

Banker files hold whitespace-delimited integers. The first two values give the number of processes and resource types (1–32 and 1–16). These are followed by Available, then the Allocation matrix in row order, then the Maximum matrix in row order. Each row has one value per resource type. Counts must be nonnegative. Allocation must fit within Maximum, claims must fit within the total supply, and each total must fit in a signed int. Need is Maximum minus Allocation. A process can finish if every Need entry fits in Work. When it finishes, its Allocation goes back into Work, and the search continues until every process finishes or no further progress is possible.

## Workers, file-size limit and cleanup

In normal mode, a child computes the sum of the first ten squares and exits 0. Failure mode deliberately exits 42. The parent calls `waitpid()` to reap each child and checks whether it exited normally or was terminated by a signal.

The soft limit is the threshold that is actually enforced, and the hard limit is the ceiling for the soft limit. The limit child lowers the `RLIMIT_FSIZE` soft limit to at most 1024 bytes while keeping the hard limit. It ignores `SIGXFSZ` locally so it can inspect write return values and errno. A write may transfer some bytes before a later write fails with `EFBIG`. The initial limit determines how many bytes get written. The temporary path is unlinked once the file is open, and the open descriptor keeps the file available until the experiment ends.

Each run manages its own IPC and temporary file. `cleanup` checks and retries any pending cleanup that the current process is tracking. A fresh invocation owns no earlier IPC names and does not scan globally. The log is kept as evidence. For the screenshot and submission workflow, see [evidence/README.md](evidence/README.md).

## Tests and limitations

`make test` builds the executable and runs timed CLI assertions covering timelines, metrics, error codes, log records, IPC, Banker outcomes, child statuses, file limits, cleanup, and both successful and failing demos. Logger and demo test runs use a temporary directory, so existing evidence logs are left untouched.

The scheduler models a single CPU with one burst per job, zero context-switch cost and no I/O. Banker performs a static safety check. Each OS demonstration uses a small, fixed workload. Thread interleaving, timestamps, process IDs and actual write results may vary between runs. Abrupt termination between exclusive IPC creation and the early unlink may leave an owned name behind. Cleanup ownership is tracked only within the running invocation.

## OS Viva — Technical Explanation

- **Process vs thread.** A process has its own address space. Threads within a process share that address space, including the logger queue, but each has its own stack and execution state.
- **fork() and waitpid().** `fork()` creates a child, returning zero in the child and the child's PID in the parent. `waitpid()` collects the chosen child's status and releases its exit record.
- **FCFS, RR and Priority.** FCFS follows arrival order. RR gives ready jobs repeated turns. Priority picks the most important ready job at dispatch time.
- **Preemption.** RR can take the simulated CPU away when the quantum expires and return the job to the queue. FCFS and non-preemptive Priority let each dispatched job run through its whole burst.
- **Turnaround and waiting time.** Turnaround covers the entire span from arrival to completion. With a single burst and no blocking I/O, subtracting CPU service time from it gives waiting time.
- **Mutex vs semaphore.** A mutex has an owner and protects a critical section. A semaphore holds permits, so one process can post a permit that another process consumes.
- **Condition variables.** A wait atomically releases the mutex while blocking and reacquires it before returning. The thread rechecks its condition in a loop because a wakeup can happen before the condition it needs is actually true.
- **Producer-consumer problem.** Producers must wait for space in the bounded buffer, and the consumer must wait for items. The queue mutex and two condition variables coordinate both cases.
- **Shared memory.** A shared mapping exposes the same bytes to both processes. The IPC semaphores establish when each side may read the other's updates.
- **Race conditions.** Behavior can depend on unpredictable interleaving when concurrent operations touch shared state without coordination. Locking the logger queue keeps slot and counter updates consistent.
- **Deadlock vs unsafe state.** Deadlock means participants are blocked waiting for progress that can never occur. An unsafe state means the safety check cannot guarantee completion under all declared maximum claims, even though the current wait relationships might still allow progress.
- **Banker's safety algorithm.** Simulate finishing every process whose remaining Need fits in Work, returning its Allocation after each simulated finish. A complete order is a safe sequence.
- **Resource limits.** Linux enforces limits per process. The child changes its own file-size soft limit and then observes how the kernel handles writes.
- **Zombie processes.** A terminated child keeps an exit record until it is reaped. Each demonstration calls `waitpid()` for the child it created.
