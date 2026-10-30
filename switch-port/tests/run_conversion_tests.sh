#!/usr/bin/env bash
set -euo pipefail
task_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
task_build="$task_root/switch-port/build-host-conversion-tests"
mkdir -p "$task_build"
python3 "$task_root/switch-port/entry/generate_entry.py" --out "$task_build/entry"
python3 - "$task_build" <<'PY'
from pathlib import Path
import re,sys
build=Path(sys.argv[1]);s=(build/'entry/entry_slice.cpp').read_text();names=['0166C0E8','01664E60','00010C50','00B17510']
preamble=s[:re.search(r'^void func_[0-9A-F]{8}\(',s,re.M).start()]
bodies=[re.search(r'^void func_'+name+r'\(ppu_context\* ctx\) \{\n.*?^\}',s,re.M|re.S)[0] for name in names]
deps=set(re.findall(r'\bfunc_([0-9A-F]{8})\b','\n'.join(bodies)))
out=preamble+'\n'.join('void func_'+n+'(ppu_context*);' for n in sorted(deps))+'\n'
out+='\n'.join('void func_'+n+'(ppu_context*) { __builtin_trap(); }' for n in sorted(deps-set(names)))+'\n'
(build/'original_conversion.cpp').write_text(out+'\n'.join(bodies)+'\n')
PY
clang++ -std=c++20 -O1 -g -fwrapv -fno-strict-aliasing -fsanitize=address,undefined -fno-omit-frame-pointer \
    -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-variable -Wno-unused-label -Wno-unused-but-set-variable \
    -Wno-comment -Wno-parentheses-equality -I "$task_build/entry" \
    "$task_root/switch-port/tests/test_conversion.cpp" "$task_build/original_conversion.cpp" -o "$task_build/test_conversion"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$task_build/test_conversion"
