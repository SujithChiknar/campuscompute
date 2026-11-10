#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT
root=$PWD
out=$tmp/output

fail() {
    printf 'FAIL: %s\n' "$*" >&2
    cat "$out" >&2
    exit 1
}

run() {
    local expected=$1 actual=0
    shift
    timeout 15 "$root/campuscompute" "$@" >"$out" 2>&1 || actual=$?
    [[ $actual == "$expected" ]] || fail "$* exited $actual (expected $expected)"
}

has() {
    grep -Fqx -- "$1" "$out" || fail "missing line: $1"
}

contains() {
    grep -Fq -- "$1" "$out" || fail "missing text: $1"
}

timeline() {
    local actual
    actual=$(sed -n '/^\[/p' "$out")
    [[ $actual == "$1" ]] || fail "unexpected timeline"
}

run 0 help
contains 'schedule fcfs|rr|priority'
run 0 jobs tests/jobs.csv
has '3 4 2 1 2 NEW'
cp "$out" "$tmp/file-jobs"
run 0 jobs
cmp -s "$out" "$tmp/file-jobs" || fail 'built-in sample differs from fixture'

run 0 schedule fcfs tests/jobs.csv
timeline $'[0,2) IDLE\n[2,7) J1\n[7,10) J2\n[10,12) J3\n[12,15) IDLE\n[15,16) J4'
has '2 10 8 5 COMPLETED'
has '3 12 8 6 COMPLETED'
run 0 schedule rr tests/jobs.csv 2
timeline $'[0,2) IDLE\n[2,4) J1\n[4,6) J2\n[6,8) J3\n[8,10) J1\n[10,11) J2\n[11,12) J1\n[12,15) IDLE\n[15,16) J4'
has '1 12 10 5 COMPLETED'
has '2 11 9 6 COMPLETED'
cp "$out" "$tmp/explicit-rr"
run 0 schedule rr tests/jobs.csv
cmp -s "$out" "$tmp/explicit-rr" || fail 'default quantum differs from 2'
run 0 schedule priority tests/jobs.csv
timeline $'[0,2) IDLE\n[2,5) J2\n[5,7) J3\n[7,12) J1\n[12,15) IDLE\n[15,16) J4'
has '1 12 10 5 COMPLETED'
has '3 7 3 1 COMPLETED'
run 0 compare tests/jobs.csv
awk '{$1=$1; print}' "$out" >"$tmp/normalized"
for row in 'FCFS 2.75 5.50 16 68.75% 0.2500' \
           'Round Robin 3.25 6.00 16 68.75% 0.2500' \
           'Priority 1.50 4.25 16 68.75% 0.2500'; do
    grep -Fxq "$row" "$tmp/normalized" || fail "wrong metrics for $row"
done

# Isolate the logger from any existing evidence log.
cd "$tmp"
run 0 logger
has 'Produced=10 Consumed=10 Final queue=0 Completed producers=2'
has 'All started threads joined successfully'
[[ $(wc -l < logs/campuscompute.log) == 10 ]] || fail 'wrong number of log records'
for producer in 1 2; do
    for type in SUBMIT READY DISPATCH RUN COMPLETE; do
        [[ $(grep -Ec "Z $type +producer=$producer " logs/campuscompute.log) == 1 ]] ||
            fail "missing or duplicate $type event for producer $producer"
    done
done
run 0 ipc
contains 'Parent received 4 CPU slices'
contains 'exit status 0'
has 'Shared-memory cleanup verified'
has 'Semaphore cleanup verified'
run 0 limit
contains 'Applied soft limit='
contains 'errno='
contains 'exit status 0'
has 'Temporary file removed'
run 0 cleanup
contains 'Cleanup verified:'
[[ -f logs/campuscompute.log ]] || fail 'cleanup deleted activity log'

cd "$root"
run 0 banker tests/banker_safe.txt
has 'SAFE STATE'
contains 'Safe sequence:'
contains 'After P'
run 0 banker tests/banker_unsafe.txt
has 'UNSAFE STATE'
contains 'P0 cannot finish'
contains 'P1 cannot finish'
run 0 worker normal
contains 'sum of squares from 1 to 10 = 385'
contains 'exit status 0'
run 0 worker fail
contains 'CONTROLLED SIMULATED FAILURE'
contains 'exit status 42'
run 2 nonsense
contains 'Unknown command'
run 2
contains 'Missing command'
run 2 schedule rr tests/jobs.csv 0
contains 'Quantum must be a positive integer'
run 2 schedule rr tests/jobs.csv 999999999999999999999
contains 'Quantum must be a positive integer'
printf '1,0,2\n' >"$tmp/bad.csv"
run 1 jobs "$tmp/bad.csv"
contains 'expected four CSV fields'
printf '1,0,2,0\n1,4,3,1\n' >"$tmp/bad.csv"
run 1 jobs "$tmp/bad.csv"
contains 'duplicate job ID'
printf '1 1\n0\n1\n0\n' >"$tmp/bad-banker"
run 1 banker "$tmp/bad-banker"
contains 'invalid Banker'

cd "$tmp"
cp -r "$root/tests" .
run 0 demo
has 'Demo summary'
[[ $(grep -c ' PASS$' "$out") == 10 ]] || fail 'demo did not report ten passing modules'
mv tests/jobs.csv tests/jobs.missing
run 1 demo
grep -Eq 'Job loading +FAIL$' "$out" || fail 'demo did not report failed job loading'
printf 'All CampusCompute tests passed.\n'
