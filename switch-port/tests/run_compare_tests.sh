#!/usr/bin/env bash
set -euo pipefail
task_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
task_build="$task_root/switch-port/build-host-compare-tests"
mkdir -p "$task_build"
python3 "$task_root/switch-port/entry/generate_entry.py" --out "$task_build/entry" --include-frontier
python3 - "$task_build" "$task_root" <<'PY'
from pathlib import Path
import re,struct,sys
build=Path(sys.argv[1]);root=Path(sys.argv[2])
source=(build/"entry/entry_slice.cpp").read_text()
match=re.search(r"^void func_01668E50\(ppu_context\* ctx\) \{\n.*?^\}",source,re.M|re.S);assert match
preamble=source[:re.search(r"^void func_[0-9A-F]{8}\(",source,re.M).start()]
(build/"original_compare.cpp").write_text(preamble+'void func_01668F98(ppu_context*);\n'+match[0]+'\n')
b=(root/"upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF").read_bytes()
ph=struct.unpack_from('>Q',b,32)[0];ps,n=struct.unpack_from('>HH',b,54);mask=0x19D3BAC+0x47AC
observed=None
for i in range(n):
    kind,flags,off,addr,_,size,_,_=struct.unpack_from('>IIQQQQQQ',b,ph+i*ps)
    if kind==1 and addr<=mask and mask+8<=addr+size:observed=struct.unpack_from('>Q',b,off+mask-addr)[0]
assert observed==0x7F7F7F7F7F7F7F7F,"Standalone mask must match the verified guest ELF"
PY
clang++ -std=c++20 -O1 -g -fwrapv -fno-strict-aliasing -fsanitize=address,undefined -fno-omit-frame-pointer \
    -Wall -Wextra -Werror -Wno-unused-function -Wno-comment -I "$task_build/entry" \
    "$task_root/switch-port/tests/test_compare.cpp" "$task_build/original_compare.cpp" -o "$task_build/test_compare"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$task_build/test_compare"
