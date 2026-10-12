# Linux Process Supervisor

A small C++17/Linux process manager designed for interview practice around OS concepts and object-oriented design patterns.

## What it demonstrates

- `fork`, `execvp`, `waitpid`
- `pipe`, `dup2`, stdout/stderr capture
- `SIGTERM` / `SIGKILL`
- parent/child resource ownership and file-descriptor closing
- background monitoring threads and synchronization
- process lifecycle management

## Design patterns

1. **Command**: CLI operations (`start`, `stop`, `kill`, `list`, `status`) implement `ICommand`.
2. **Strategy**: restart behavior implements `IRestartPolicy` (`NeverRestart`, `AlwaysRestart`, `OnFailureBackoff`).
3. **Observer**: `ConsoleObserver` and `MetricsObserver` receive process events.
4. **Factory**: `RestartPolicyFactory` and `CommandFactory` centralize object creation.

## Build

Requirements: Linux or WSL, CMake 3.16+, a C++17 compiler, and POSIX threads.

```bash
cmake -S . -B build
cmake --build build -j
./build/process_supervisor
```

Try:

```text
start heartbeat
list
add bad on-failure /bin/sh -c "echo fail; exit 2"
start bad
status bad
stop bad
quit
```

Arguments containing spaces can be quoted; the command-line parser preserves
them when it constructs the child process argument vector.

## Test

From this directory:

```bash
bash tests/smoke_test.sh
```

The smoke test builds the executable, starts the built-in heartbeat process,
lists it, and verifies graceful shutdown through the CLI.

## Interview talking points

- Why `dup2(pipefd[1], STDOUT_FILENO)` redirects the child output.
- Why the parent must close the write end of the pipe, otherwise EOF never arrives.
- `fork()` duplicates the process image; `execvp()` replaces the child process image.
- A process has a separate virtual address space; threads share address space.
- `waitpid()` reaps children and prevents zombies.
- Graceful stop first sends `SIGTERM`; after a timeout, the manager escalates to `SIGKILL`.
- Restart policy is separated using Strategy so the process code does not contain a large `if/else` policy block.
- Event consumers are separated by Observer, so adding logging/metrics does not modify `ManagedProcess`.

## Production caveat

This project intentionally uses `fork()` because it is useful for learning process semantics. In a large multithreaded production program, `posix_spawn()` can be a safer and simpler choice because the child side of `fork()` must be very careful about async-signal-safety before `exec()`.
