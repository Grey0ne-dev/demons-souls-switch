#!/usr/bin/env bash
set -euo pipefail
task_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
task_build="$task_root/switch-port/build-host-copy-tests"
mkdir -p "$task_build"
python3 "$task_root/switch-port/entry/generate_entry.py" --out "$task_build/entry" --include-frontier
python3 - "$task_build" <<'PY'
from pathlib import Path
import re,sys
build=Path(sys.argv[1])
source=(build/"entry/entry_slice.cpp").read_text()
match=re.search(r"^void func_01666594\(ppu_context\* ctx\) \{\n.*?^\}",source,re.M|re.S)
assert match
preamble=source[:re.search(r"^void func_[0-9A-F]{8}\(",source,re.M).start()]
(build/"original_copy.cpp").write_text(preamble+match[0]+'\n')
PY
clang++ -std=c++20 -O1 -g -fwrapv -fno-strict-aliasing -fsanitize=address,undefined -fno-omit-frame-pointer \
    -Wall -Wextra -Werror -Wno-unused-function -Wno-comment -I "$task_build/entry" \
    "$task_root/switch-port/tests/test_copy.cpp" "$task_build/original_copy.cpp" -o "$task_build/test_copy"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$task_build/test_copy"
