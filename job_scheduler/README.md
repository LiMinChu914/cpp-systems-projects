# Concurrent Job Scheduler

A C++17/Linux thread-pool scheduler built for OS + OOP interview practice.

## OS/concurrency topics

- worker thread pool
- producer/consumer queue
- `std::mutex` and `std::condition_variable`
- `std::atomic`
- cooperative cancellation
- race-condition avoidance
- CPU-bound vs I/O-bound tasks
- optional Linux CPU affinity via `pthread_setaffinity_np`

## Design patterns

1. **Command**: every work item implements `ITask::execute()`; CLI commands also use Command.
2. **Strategy**: scheduling policy is swappable at runtime: FIFO, priority, fair-priority.
3. **Observer**: logging and metrics subscribe to scheduler events.
4. **Factory**: task construction and scheduling-strategy creation are centralized.

## Build

Requirements: Linux or WSL, CMake 3.16+, a C++17 compiler, and POSIX threads.

```bash
cmake -S . -B build
cmake --build build -j
./build/job_scheduler 4
```

Optional Linux affinity demo:

```bash
./build/job_scheduler 4 --pin
```

Commands:

```text
sleep slow 1000 1
sleep urgent 100 10
primes cpu 100000 5
checksum file /etc/hosts 3
strategy priority
list
cancel 1
pause
resume
quit
```

## Test

From this directory:

```bash
bash tests/smoke_test.sh
```

The smoke test builds the executable, submits I/O-bound and CPU-bound work,
changes scheduling strategy, and checks that the scheduler shuts down cleanly.

## Interview talking points

### Mutex vs semaphore

The shared task queue is protected by a mutex because only one thread at a time may modify its internal state. A counting semaphore would be useful to represent N identical resources, but mutual exclusion for the container itself is naturally modeled by a mutex.

### Why condition_variable?

Workers should sleep while there is no work. Busy-waiting wastes CPU. `condition_variable::wait` atomically releases the mutex while the thread sleeps and reacquires it before returning.

### Why the predicate?

Condition variables may wake spuriously. The predicate re-checks the actual scheduler state and queue condition under the mutex.

### Cancellation

C++ does not safely support forcibly killing arbitrary worker threads. This project therefore uses cooperative cancellation: a shared atomic flag is set, and long-running tasks periodically check it.

### Strategy pattern

Worker code does not care whether the next task is FIFO, highest priority, or aged priority. It asks an `ISchedulingStrategy`. New policies can be added without rewriting the thread pool.

### Avoiding starvation

Pure priority scheduling can starve low-priority work. `FairPriorityScheduling` adds an aging component so waiting tasks gradually become more competitive.

### CPU affinity

Pinning is optional. Affinity can improve cache locality in some real-time-ish workloads, but can also reduce OS scheduling flexibility. It should be measured rather than enabled blindly.

## Why this is relevant to a controller-system role

A controller often has periodic control work, communication work, logging, alarms, and background maintenance competing for CPU time. The project is not a real-time scheduler, but it gives you a concrete platform to discuss priority, latency, shared-state protection, starvation, multicore behavior, and how a common task interface simplifies system architecture.
