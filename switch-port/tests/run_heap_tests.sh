#!/usr/bin/env bash
set -euo pipefail
task_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
task_build="$task_root/switch-port/build-host-heap-tests"
mkdir -p "$task_build"
clang -std=c17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror \
    -I "$task_root/switch-port/include" -I "$task_root/switch-port/entry" \
    "$task_root/switch-port/entry/ds_heap.c" "$task_root/switch-port/source/ds_image.c" \
    "$task_root/switch-port/tests/test_heap.c" -Wl,--wrap=calloc -o "$task_build/test_heap"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$task_build/test_heap"
