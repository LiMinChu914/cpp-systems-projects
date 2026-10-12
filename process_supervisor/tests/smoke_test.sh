#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cmake -S "$root" -B "$root/build" >/dev/null
cmake --build "$root/build" -j >/dev/null
printf 'start heartbeat\nlist\nstop heartbeat\nquit\n' | "$root/build/process_supervisor"
