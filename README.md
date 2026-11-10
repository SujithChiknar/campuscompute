# CampusCompute

**Intelligent Job Scheduling and Resource Management**  
UE25CS242A — Operating Systems

CampusCompute models jobs in a university computing lab. CPU scheduling is a deterministic simulation on a logical clock. The logger uses real POSIX threads, and the IPC, worker and file-size experiments use real Linux child processes.

## Build on Ubuntu or WSL Ubuntu

From Windows, if Ubuntu is absent, run `wsl --install -d Ubuntu` in an administrator PowerShell and complete Ubuntu's setup. In the Ubuntu terminal, change to this project directory. A Windows checkout under `C:\Users` is accessible under `/mnt/c/Users`.

```bash
sudo apt update
sudo apt install build-essential zip unzip
make clean
make
make test
```

GCC, Make, Bash, and standard Ubuntu command-line tools are used. ZIP tools are for packaging. `make rebuild` performs a clean build. `make clean` removes the executable, objects and dependency files.

## Commands

Run these from the project directory.

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

For jobs, schedule and compare, omit the CSV or pass `-` to use the repeatable built-in sample. For example, `./campuscompute schedule rr - 3` selects the sample with quantum 3. Exit status 0 indicates successful execution, including a demonstrated unsafe Banker state or a worker that deliberately exits 42. Malformed data or operational failure returns 1. CLI usage errors return 2.

## Input and scheduling

Job CSV has an optional exact header `id,arrival,burst,priority` and one unquoted record per line. The loader accepts 1–128 jobs with unique positive integer IDs, nonnegative integer arrivals, bursts of 1–10,000 and priorities of 0–99. IDs and arrivals fit in a signed C int. Logical times use 64-bit integers so later completions can exceed the arrival range.

- FCFS takes the earliest arrival. Input order breaks arrival ties. Each record has a unique input position, so an ID tie-break is never needed.
- Round Robin uses a FIFO ready queue and a positive integer quantum, default 2. After a slice, newly arrived jobs join the tail in arrival and input order, including arrivals exactly at the slice boundary. A preempted job then joins behind them.
- Non-preemptive priority picks the lowest priority number among ready jobs and breaks ties by arrival and input order. A running job keeps the CPU until it finishes.

A timeline row `[a,b)` covers logical times from a inclusive to b exclusive. Idle intervals appear explicitly. CT is the time a job finishes, TAT is CT minus arrival, and WT is TAT minus burst. Elapsed time starts at zero and ends at the last completion. Utilization divides busy time by elapsed time. Throughput divides completed jobs by elapsed time. Compare runs each policy on a private copy of the same jobs and uses RR quantum 2.

## Logger, IPC and resource safety

The logger appends to `logs/campuscompute.log`. Two producer threads each emit five lab-job lifecycle events. A mutex protects the three-slot circular queue and its counters. The producers wait on not-full and the consumer waits on not-empty, checking the predicate again after waking. The consumer releases the mutex before writing each event to the log file. Main joins the producers, closes the queue and wakes the consumer. The consumer drains any remaining events before main joins it.

IPC creates exclusive shared-memory and named semaphore objects and opens their handles before forking. The names are removed once the handles exist. The inherited mapping and semaphore handles keep the objects alive until both processes release them. The parent publishes the job and posts the request semaphore. The child waits for the request, computes the number of two-unit CPU slices and posts the reply. The parent reaps the child and waits on the reply semaphore before reading the result.

Banker files contain whitespace-delimited integers. The first two values give the number of processes and resource types (1–32 and 1–16). Then supply Available, the Allocation matrix in row order, and the Maximum matrix in row order. Each row has one value per resource type. Counts are nonnegative. Allocation must fit within Maximum, claims must fit within the total supply, and each total must fit in a signed int. Need is Maximum minus Allocation. A process can finish if every Need entry fits in Work. Its Allocation is returned to Work, and the search continues until every process finishes or no more progress is possible.

## Workers, file-size limit and cleanup

Normal mode computes the sum of the first ten squares in a child and exits 0. Failure mode deliberately exits 42. The parent uses `waitpid()` to reap each child and checks whether it exited or was terminated by a signal.

The soft limit is the enforced threshold and the hard limit is the ceiling for the soft limit. The limit child lowers the `RLIMIT_FSIZE` soft limit to at most 1024 bytes, preserving the hard limit. It ignores `SIGXFSZ` locally so it can inspect write return values and errno. A write may transfer some bytes before a later write fails with `EFBIG`. The initial limit affects how many bytes are written. The temporary path is unlinked once the file is open. The open descriptor keeps the file available until the experiment ends.

Each run manages its own IPC and temporary file. `cleanup` checks and retries any pending cleanup tracked by the current process. A fresh invocation owns no earlier IPC names and performs no global scan. The log persists for evidence. See [evidence/README.md](evidence/README.md) for the screenshot and submission workflow.

## Tests and limitations

`make test` builds the executable and runs timed CLI assertions for timelines, metrics, error codes, log records, IPC, Banker outcomes, child statuses, file limits, cleanup and demo success and failure. Logger and demo test runs use a temporary directory so existing evidence logs are preserved.

The scheduler models one CPU and one burst per job with zero context-switch cost and no I/O. Banker performs a static safety check. Each OS demonstration uses a small, fixed workload. Thread interleaving, timestamps, process IDs and actual write results can vary. Abrupt termination between exclusive IPC creation and early unlink may leave an owned name behind. The program tracks cleanup ownership only within the running invocation.

## OS Viva — Technical Explanation

- **Process vs thread.** A process has its own address space. Threads inside a process share the address space, including the logger queue, and each thread has its own stack and execution state.
- **fork() and waitpid().** `fork()` creates a child and returns zero in the child and the child PID in the parent. `waitpid()` collects the selected child's status and releases its exit record.
- **FCFS, RR and Priority.** FCFS uses arrival order. RR gives ready jobs repeated turns. Priority chooses the most important ready job at dispatch.
- **Preemption.** RR can take the simulated CPU away when the quantum ends and put the job back in the queue. FCFS and non-preemptive Priority let each dispatch finish its burst.
- **Turnaround and waiting time.** Turnaround measures the entire arrival-to-completion period. With one burst and no blocking I/O, subtracting CPU service time gives waiting time.
- **Mutex vs semaphore.** A mutex has an owner and protects a critical section. A semaphore holds permits, and one process can post a permit for another process to consume.
- **Condition variables.** A wait atomically releases the mutex while blocking and reacquires it before returning. The thread checks its condition in a loop because a wakeup can occur before the condition it needs is true.
- **Producer-consumer problem.** Producers must wait for space in the bounded buffer, while the consumer must wait for items. The queue mutex and two condition variables coordinate both cases.
- **Shared memory.** A shared mapping exposes the same bytes in both processes. The IPC semaphores establish when each side can read the other's updates.
- **Race conditions.** Behavior can depend on unpredictable interleaving when concurrent operations access shared state without coordination. Locking the logger queue keeps slot and counter updates consistent.
- **Deadlock vs unsafe state.** Deadlock means participants are blocked waiting for progress that cannot occur. An unsafe state means the safety check cannot guarantee completion under all declared maximum claims. The current wait relationships may still permit progress.
- **Banker's safety algorithm.** Simulate finishing every process whose remaining Need fits in Work, returning its Allocation after each simulated finish. A complete order is a safe sequence.
- **Resource limits.** Linux enforces per-process limits. The child changes its own file-size soft limit and then observes kernel write behavior.
- **Zombie processes.** A terminated child retains an exit record until it is reaped. Each demonstration calls `waitpid()` for the child it created.
