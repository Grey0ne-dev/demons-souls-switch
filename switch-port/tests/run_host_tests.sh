#!/usr/bin/env bash
set -euo pipefail
task_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
task_build="$task_root/switch-port/build-host-tests"
mkdir -p "$task_build"
clang -std=c17 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -O1 -g \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$task_root/switch-port/include" \
    "$task_root/switch-port/tests/test_image.c" \
    "$task_root/switch-port/source/ds_image.c" -lcrypto \
    -o "$task_build/test_image"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
    "$task_build/test_image" "$task_root/upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF"
