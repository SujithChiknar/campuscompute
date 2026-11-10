# Genuine screenshot evidence

Open interactive Bash in WSL Ubuntu, change into the project directory and set your actual complete SRN before taking any screenshots.

```bash
export SRN='REPLACE_WITH_YOUR_ACTUAL_FULL_SRN'
export PS1='[$SRN] \w\$ '
```

Replace the placeholder with your full university SRN. Keep the SRN visible in the terminal prompt in **every screenshot**. Capture the real command and the relevant real output together. Adjust the terminal height or font size before capturing long output. Execute the commands yourself in order. Save the captures as S01 through S24.

## S01 — Environment and SRN prompt

```bash
printf 'SRN: %s\n' "$SRN"; uname -sr; cat /etc/os-release
```

**Evidence** — SRN prompt and Ubuntu identification.

The shell identifies the Linux environment used for the project.

## S02 — Compiler and build tools

```bash
gcc --version | head -n 1; make --version | head -n 1
```

**Evidence** — Compiler and Make versions.

GCC compiles the modules and Make coordinates the build.

## S03 — Project files

```bash
find include src tests evidence -maxdepth 1 -type f ! -name '*.o' ! -name '*.d' | sort; ls -l Makefile README.md
```

**Evidence** — Source, headers, fixtures and documentation.

Each OS feature has a dedicated module.

## S04 — Clean build

```bash
make clean && make
```

**Evidence** — Fresh GCC compilation and link.

All modules are compiled into one executable.

## S05 — CLI help

```bash
./campuscompute help
```

**Evidence** — Command syntax.

The CLI exposes each feature independently.

## S06 — FCFS scheduling

```bash
./campuscompute schedule fcfs tests/jobs.csv
```

**Evidence** — Timeline, idle intervals and job metrics.

Earlier arrivals run first, with input order breaking ties.

## S07 — Round Robin scheduling

```bash
./campuscompute schedule rr tests/jobs.csv 2
```

**Evidence** — Time slices, preemption and metrics.

A job that still needs CPU time rejoins the ready queue.

## S08 — Priority scheduling

```bash
./campuscompute schedule priority tests/jobs.csv
```

**Evidence** — Priority dispatch order and metrics.

The highest-priority ready job runs to completion.

## S09 — Scheduling comparison

```bash
./campuscompute compare tests/jobs.csv
```

**Evidence** — Three metric rows for the same workload.

Average waiting and turnaround times differ by policy.

## S10 — Producer-consumer logger

```bash
./campuscompute logger
```

**Evidence** — Enqueue and dequeue trace.

Two producers hand activity records to one consumer.

## S11 — Thread synchronization and termination

```bash
./campuscompute logger | tail -n 8
```

**Evidence** — Queue activity, totals and joins.

The consumer drains the queue before all threads finish.

## S12 — Shared-memory IPC

```bash
./campuscompute ipc
```

**Evidence** — Parent and child PIDs, job data and result.

Both processes exchange the job through a shared mapping.

## S13 — Semaphore coordination and cleanup

```bash
./campuscompute ipc && ./campuscompute cleanup
```

**Evidence** — Semaphore operations, child exit and cleanup.

The request and reply signals coordinate shared-memory access.

## S14 — Banker's safe state

```bash
./campuscompute banker tests/banker_safe.txt
```

**Evidence** — Need matrix, Work progression and safe sequence.

Each simulated completion returns resources to Work.

## S15 — Banker's unsafe state

```bash
./campuscompute banker tests/banker_unsafe.txt
```

**Evidence** — Unsafe classification and unfinished processes.

Neither process can meet its remaining claim with the current Work vector.

## S16 — Normal worker

```bash
./campuscompute worker normal
```

**Evidence** — Computation and reaped exit status zero.

The parent confirms the child completed normally.

## S17 — Simulated worker failure

```bash
./campuscompute worker fail
```

**Evidence** — Controlled failure message and reaped status 42.

The parent observes the child's deliberate nonzero exit.

## S18 — Resource-limit demonstration

```bash
./campuscompute limit
```

**Evidence** — Original limits, applied soft limit, write results and cleanup.

The child observes how Linux enforces its file-size limit.

## S19 — Invalid command/input

```bash
./campuscompute unknown; printf 'exit=%s\n' "$?"; ./campuscompute schedule rr tests/jobs.csv 0; printf 'exit=%s\n' "$?"
```

**Evidence** — Usage errors and nonzero exit codes.

The CLI rejects an unknown command and a zero quantum.

## S20 — Cleanup verification

```bash
./campuscompute cleanup && ls -l logs/campuscompute.log
```

**Evidence** — Cleanup result and retained log.

Cleanup preserves the activity log.

## S21 — Integrated execution

```bash
./campuscompute demo > demo-output.txt 2>&1; demo_status=$?; tail -n 14 demo-output.txt; printf 'demo exit=%s\n' "$demo_status"
```

**Evidence** — Integrated summary and exit status.

The summary reports the result of each module invocation.

## S22 — Generated logs

```bash
tail -n 10 logs/campuscompute.log
```

**Evidence** — Timestamped event records.

The consumer records the type, producer and description.

## S23 — Final source/build files

```bash
ls -l Makefile README.md campuscompute; find include src tests -maxdepth 1 -type f | sort
```

**Evidence** — Final source files, executable and build artifacts.

Make produced the executable and per-module object files.

## S24 — Submission ZIP and integrity check

```bash
cd ..
zip -r "${SRN}_CampusCompute.zip" campuscompute/Makefile campuscompute/README.md campuscompute/include campuscompute/src campuscompute/tests -x '*.o' '*.d'
unzip -t "${SRN}_CampusCompute.zip"
cd campuscompute
```

**Evidence** — Archive creation and integrity-test result.

The archive includes source and fixtures for a clean build.

## Assemble the evidence PDF

Insert the 24 genuine captures in order in a document editor. Label each image S01–S24 and place its one-line explanation below it. Keep prompts and relevant output legible at normal zoom. Export one PDF named using your actual complete SRN followed by `.pdf`. Open the PDF and verify the numbering, all SRN prompts and all captions. Submit that PDF as the evidence artifact. A separate report is unnecessary.

S24 archives project sources, documentation and test fixtures. The evidence directory, generated logs, demo transcript, executable and compiler artifacts are excluded. Include evidence in the ZIP only if the submission instructions explicitly require it. Use a fresh archive name or move an older archive aside before repeating S24, since updating an existing ZIP can retain obsolete entries.
