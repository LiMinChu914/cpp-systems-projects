# C++ Systems Programming Projects

Two self-contained C++17/Linux projects that demonstrate operating-system
fundamentals, concurrency, and practical object-oriented design patterns.

## Projects

| Project | Focus | Key patterns |
| --- | --- | --- |
| [Concurrent Job Scheduler](job_scheduler/) | Thread pool, producer/consumer queue, condition variables, cooperative cancellation, priorities, and optional CPU affinity | Command, Strategy, Observer, Factory |
| [Linux Process Supervisor](process_supervisor/) | `fork`/`execvp`, pipes, output capture, signals, `waitpid`, process lifecycle, and restart policies | Command, Strategy, Observer, Factory |

Both projects provide an interactive CLI and a smoke test. They are learning
projects rather than production replacements for a real-time scheduler or an
init system such as systemd.

## Requirements

- Linux or WSL
- A C++17 compiler (GCC or Clang)
- CMake 3.16 or newer
- POSIX threads

## Quick start

Build and run the scheduler:

```bash
cmake -S job_scheduler -B job_scheduler/build
cmake --build job_scheduler/build -j
./job_scheduler/build/job_scheduler 4
```

Build and run the supervisor:

```bash
cmake -S process_supervisor -B process_supervisor/build
cmake --build process_supervisor/build -j
./process_supervisor/build/process_supervisor
```

Enter `help` in either program to list the available commands. See each
project's README for examples and implementation notes.

## Tests

Run the end-to-end smoke tests from the repository root:

```bash
bash job_scheduler/tests/smoke_test.sh
bash process_supervisor/tests/smoke_test.sh
```

The tests configure and build their respective project before exercising its
CLI and lifecycle behavior.

## Repository layout

```text
.
├── job_scheduler/
│   ├── include/
│   ├── src/
│   └── tests/
└── process_supervisor/
    ├── include/
    ├── src/
    └── tests/
```

## Design notes

- **Command** gives CLI actions and jobs a consistent execution interface.
- **Strategy** keeps scheduling and restart decisions separate from lifecycle
  management.
- **Observer** decouples console logging and metrics from the core classes.
- **Factory** centralizes construction from user-facing names and commands.

The implementations favor RAII, explicit ownership, and cooperative shutdown
so that threads, processes, pipes, and other resources have clear lifetimes.
