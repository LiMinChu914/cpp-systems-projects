#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cmake -S "$root" -B "$root/build" >/dev/null
cmake --build "$root/build" -j >/dev/null
{
  echo 'sleep slow 120 1'
  echo 'sleep fast 20 10'
  echo 'primes cpu 3000 5'
  echo 'strategy priority'
  echo 'list'
  sleep 0.3
  echo 'list'
  echo 'quit'
} | "$root/build/job_scheduler" 2
