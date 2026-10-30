#!/usr/bin/env bash
set -euo pipefail
task_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
task_build="$task_root/switch-port/build-host-entry-tests"
task_generator_options=()
if [[ "${1:-}" == --explore-* ]]; then
    task_build="$task_root/switch-port/build-host-explore-tests"
    task_generator_options=(--annotate-indirect-calls)
fi
if [[ "${1:-}" == "--explore-ahead" || "${1:-}" == --explore-twelfth* ]]; then
    task_generator_options+=(--include-frontier)
fi
if [[ "${1:-}" == --explore-published* ]]; then
    task_generator_options+=(--include-frontier)
fi
if [[ "${1:-}" == --explore-stream-locale* ]]; then
    task_generator_options+=(--include-frontier)
fi
if [[ "${1:-}" == --explore-first-facet* ]]; then
    task_generator_options+=(--include-frontier)
fi
task_generated="$task_build/entry"
mkdir -p "$task_build"
python3 "$task_root/switch-port/entry/generate_entry.py" --out "$task_generated" "${task_generator_options[@]}"
if [[ "${1:-}" == --explore-wide-streams-*-fault || "${1:-}" == --explore-wide-streams-faults ]]; then
    python3 - "$task_generated/entry_slice.cpp" <<'PYFAULT'
from pathlib import Path
import sys
p=Path(sys.argv[1]);source=p.read_text()
hooks=[('DS_FRAME(0x0168ABD0);','ds_test_wide_fault(ctx,0); DS_FRAME(0x0168ABD0);'),
 ('ds_entry_indirect_callsite(ctx, 0x016B84F4u);','ds_test_wide_fault(ctx,1); ds_entry_indirect_callsite(ctx, 0x016B84F4u);'),
 ('ctx->lr = 0x01684D90; func_0166F990(ctx); DRAIN_TRAMPOLINE(ctx);','ctx->lr = 0x01684D90; func_0166F990(ctx); DRAIN_TRAMPOLINE(ctx); ds_test_wide_fault(ctx,2);'),
 ('ds_entry_indirect_callsite(ctx, 0x0168E690u);','ds_test_wide_fault(ctx,3); ds_entry_indirect_callsite(ctx, 0x0168E690u);'),
 ('DS_FRAME(0x0166C0E8);','ds_test_wide_fault(ctx,4); DS_FRAME(0x0166C0E8);'),
 ('ctx->lr = 0x016A0530; func_01676258(ctx); DRAIN_TRAMPOLINE(ctx);','ctx->lr = 0x016A0530; func_01676258(ctx); DRAIN_TRAMPOLINE(ctx); ds_test_wide_fault(ctx,5);'),
 ('ctx->lr = 0x00B17548; func_00010C50(ctx); DRAIN_TRAMPOLINE(ctx);','ctx->lr = 0x00B17548; func_00010C50(ctx); DRAIN_TRAMPOLINE(ctx); ds_test_wide_fault(ctx,6);')]
for old,new in hooks:
    assert source.count(old)==1,old
    source=source.replace(old,new)
source=source.replace('#include "ppu_recomp.h"','#include "ppu_recomp.h"\nextern void ds_test_wide_fault(ppu_context*,unsigned);')
p.write_text(source)
PYFAULT
fi
if [[ "${1:-}" == --explore-narrow-streams-*-fault || "${1:-}" == --explore-narrow-streams-faults ]]; then
    python3 - "$task_generated/entry_slice.cpp" <<'PYFAULT'
from pathlib import Path
import sys
p=Path(sys.argv[1]);source=p.read_text()
for old,new in [
 ('DS_FRAME(0x0168ABD0);','ds_test_narrow_fault(ctx,0); DS_FRAME(0x0168ABD0);'),
 ('ds_entry_checkpoint_function(0x0168BC68u);','ds_test_narrow_fault(ctx,1); ds_entry_checkpoint_function(0x0168BC68u);'),
 ('ctx->lr = 0x01684D90; func_0166F990(ctx); DRAIN_TRAMPOLINE(ctx);','ctx->lr = 0x01684D90; func_0166F990(ctx); DRAIN_TRAMPOLINE(ctx); ds_test_narrow_fault(ctx,2);'),
 ('ds_entry_indirect_callsite(ctx, 0x016872A0u);','ds_test_narrow_fault(ctx,3); ds_entry_indirect_callsite(ctx, 0x016872A0u);')]:
    assert source.count(old)==1,old
    source=source.replace(old,new)
source=source.replace('#include "ppu_recomp.h"','#include "ppu_recomp.h"\nextern void ds_test_narrow_fault(ppu_context*,unsigned);')
p.write_text(source)
PYFAULT
fi
if [[ "${1:-}" == --explore-first-stream-*-fault || "${1:-}" == --explore-first-stream-faults ]]; then
    python3 - "$task_generated/entry_slice.cpp" <<'PYFAULT'
from pathlib import Path
import sys
p=Path(sys.argv[1]);source=p.read_text()
hooks={
    "ctx->lr = 0x01688448; func_01684D70(ctx); DRAIN_TRAMPOLINE(ctx);":0,
    "ds_entry_indirect_callsite(ctx, 0x01688280u); DRAIN_TRAMPOLINE(ctx);":1,
    "ctx->lr = 0x01684D90; func_0166F990(ctx); DRAIN_TRAMPOLINE(ctx);":2,
}
for old,stage in hooks.items():
    assert source.count(old)==1
    source=source.replace(old,old+f" ds_test_first_stream_fault(ctx,{stage});")
old="ds_entry_indirect_callsite(ctx, 0x01688280u); DRAIN_TRAMPOLINE(ctx);"
source=source.replace(old,"ds_test_first_stream_fault(ctx,4); "+old)
old="ctx->lr = 0x01685FEC; func_0168ABD0(ctx); DRAIN_TRAMPOLINE(ctx);"
assert source.count(old)==1
source=source.replace(old,"ctx->lr = 0x01685FEC; ds_test_first_stream_fault(ctx,3); func_0168ABD0(ctx); DRAIN_TRAMPOLINE(ctx);")
# The declaration follows ppu_context's definition; production output is untouched.
needle='#include "ppu_recomp.h"'
assert source.count(needle)==1
source=source.replace(needle,needle+'\nextern void ds_test_first_stream_fault(ppu_context*,unsigned);')
p.write_text(source)
PYFAULT
fi
if [[ "${1:-}" == "--explore-bad-callsite" ]]; then
    python3 - "$task_generated/entry_slice.cpp" <<'PYFAULT'
from pathlib import Path
import sys
p=Path(sys.argv[1]); source=p.read_text()
a="ds_entry_indirect_callsite(ctx, 0x016B84F4u)"
assert source.count(a)==1
p.write_text(source.replace(a,"ds_entry_indirect_callsite(ctx, 0x016B84F8u)"))
PYFAULT
fi
if [[ "${1:-}" == "--explore-fourth-fault" ]]; then
    python3 - "$task_generated/entry_slice.cpp" <<'PYFAULT'
from pathlib import Path
import sys
p = Path(sys.argv[1]); source = p.read_text()
old = "ctx->gpr[11] = (int64_t)(int32_t)(0x64);\n        lv2_syscall(ctx);"
assert source.count(old) == 1
# Only fail after the last real native create in constructor four succeeds.
p.write_text(source.replace(old, old + " if (vm_read32(0x1AD9F84u) == 0x1004u) vm_write32(0u, 1u);"))
PYFAULT
fi
if [[ "${1:-}" == "--explore-sixth-state-fault" ]]; then
    python3 - "$task_generated/entry_slice.cpp" <<'PYFAULT'
from pathlib import Path
import sys
p = Path(sys.argv[1]); source = p.read_text()
old = "ctx->lr = 0x0006B8D0; func_00ADE478(ctx); DRAIN_TRAMPOLINE(ctx);"
assert source.count(old) == 1
p.write_text(source.replace(old, old + " vm_write32(0x1AD8DA4u, 0u);"))
PYFAULT
fi
if [[ "${1:-}" == "--explore-ninth-state-fault" || "${1:-}" == "--explore-ninth-native-fault" ]]; then
    python3 - "$task_generated/entry_slice.cpp" "$1" <<'PYFAULT'
from pathlib import Path
import re, sys
p = Path(sys.argv[1]); source = p.read_text()
if sys.argv[2] == "--explore-ninth-native-fault":
    old = "ctx->gpr[11] = (int64_t)(int32_t)(0x64);\n        lv2_syscall(ctx);"
    assert source.count(old) == 1
    source = source.replace(old, old + " if (vm_read32(0x1AD9FF4u) == 0x1005u) vm_write32(0u, 1u);")
else:
    match = re.search(r"^void func_008B8600\(ppu_context\* ctx\) \{\n.*?^\}", source, re.M | re.S)
    assert match
    old = "ctx->gpr[2] = vm_read64(ctx->gpr[1] + 0x28);"
    assert match[0].count(old) == 3
    parts = match[0].rsplit(old, 1)
    body = parts[0] + old + " vm_write32(0x1AD8E9Cu, 0u);" + parts[1]
    source = source[:match.start()] + body + source[match.end():]
p.write_text(source)
PYFAULT
fi
task_test_source="$task_root/switch-port/tests/test_entry.cpp"
if [[ "${1:-}" == --explore-wide-streams* ]]; then
    task_test_source="$task_root/switch-port/tests/test_wide_streams.cpp"
fi
if [[ "${1:-}" == --explore-narrow-streams* ]]; then
    task_test_source="$task_root/switch-port/tests/test_narrow_streams.cpp"
fi
if [[ "${1:-}" == --explore-first-stream* ]]; then
    task_test_source="$task_root/switch-port/tests/test_first_stream.cpp"
fi
if [[ "${1:-}" == --explore-first-facet* ]]; then
    task_test_source="$task_root/switch-port/tests/test_first_facet.cpp"
fi
if [[ "${1:-}" == --explore-first-facet-*-fault ]]; then
    python3 - "$task_generated/entry_slice.cpp" "$1" <<'PYFAULT'
from pathlib import Path
import sys
p=Path(sys.argv[1]);source=p.read_text();mode=sys.argv[2]
call="ds_entry_indirect_callsite(ctx, 0x01688280u); DRAIN_TRAMPOLINE(ctx);"
if mode in ("--explore-first-facet-state-fault","--explore-first-facet-node-fault","--explore-first-facet-snapshot-fault"):
    old=call;address={"--explore-first-facet-state-fault":"0x600000D4u","--explore-first-facet-node-fault":"0x600000F0u","--explore-first-facet-snapshot-fault":"0xFEFFB08u"}[mode]
    value="0x70000000u" if mode=="--explore-first-facet-node-fault" else "0u"
    new=f"vm_write32({address},{value}); "+old
elif mode=="--explore-first-facet-seventh-fault":
    old="ctx->lr = 0x01688380; func_0168ABD0(ctx); DRAIN_TRAMPOLINE(ctx);";new="ctx->gpr[3]=25; "+old
elif mode=="--explore-first-facet-eighth-fault":
    old="ctx->lr = 0x016884F0; func_0168ABD0(ctx); DRAIN_TRAMPOLINE(ctx);";new="ctx->gpr[3]=16; "+old
elif mode=="--explore-first-facet-descriptor-fault":
    old=call;new="vm_write32(0x197034Cu,0x19D3BACu); "+old
elif mode=="--explore-first-facet-target-fault":
    old=call;new="ctx->ctr=0x9E9648u;ctx->gpr[10]=0x19703A8u; "+old
elif mode=="--explore-first-facet-compare-fault":
    old="ctx->lr = 0x01668254; func_01668E50(ctx); DRAIN_TRAMPOLINE(ctx);";new="ctx->gpr[3]=0x70000000u; "+old
elif mode=="--explore-first-facet-lock-fault":
    old="ctx->lr = 0x0168A6F4; func_0166F990(ctx); DRAIN_TRAMPOLINE(ctx);";new=old+" vm_write32(0u,1u);"
else:raise SystemExit("Unknown first-facet fault fixture")
assert source.count(old)==1
p.write_text(source.replace(old,new))
PYFAULT
fi
if [[ "${1:-}" == --explore-stream-locale* ]]; then
    task_test_source="$task_root/switch-port/tests/test_stream_locale.cpp"
fi
if [[ "${1:-}" == --explore-stream-locale-*-fault ]]; then
    python3 - "$task_generated/entry_slice.cpp" "$1" <<'PYFAULT'
from pathlib import Path
import sys
p=Path(sys.argv[1]);source=p.read_text();mode=sys.argv[2]
if mode in ("--explore-stream-locale-state-fault","--explore-stream-locale-handle-fault"):
    old="ctx->lr = 0x01688380; func_0168ABD0(ctx); DRAIN_TRAMPOLINE(ctx);"
    address="0x60000044u" if mode=="--explore-stream-locale-state-fault" else "0x600000B0u"
    new="vm_write32("+address+",0u); "+old
elif mode in ("--explore-stream-locale-fourth-fault","--explore-stream-locale-stack-fault"):
    old="ctx->lr = 0x016A051C; func_0168ABD0(ctx); DRAIN_TRAMPOLINE(ctx);"
    mutation="ctx->gpr[3]=41;" if mode=="--explore-stream-locale-fourth-fault" else "ctx->gpr[1]-=16;"
    new="if (vm_read32(0xE000430u)) { "+mutation+" } "+old
elif mode=="--explore-stream-locale-fifth-fault":
    old="ctx->lr = 0x01686EF4; func_0168ABD0(ctx); DRAIN_TRAMPOLINE(ctx);"
    new="ctx->gpr[3]=8; "+old
elif mode=="--explore-stream-locale-sixth-fault":
    old="ctx->lr = 0x01685FEC; func_0168ABD0(ctx); DRAIN_TRAMPOLINE(ctx);"
    new="ctx->gpr[3]=8; "+old
elif mode=="--explore-stream-locale-native-fault":
    old="ctx->lr = 0x016A0530; func_01676258(ctx); DRAIN_TRAMPOLINE(ctx);"
    new=old+" if (vm_read32(0x1CE655Cu)==0x60000080u) vm_write32(0u,1u);"
elif mode=="--explore-stream-locale-tls-fault":
    old="ctx->lr = 0x01686F10; func_016896C0(ctx); DRAIN_TRAMPOLINE(ctx);"
    new="vm_write32(0xE000430u,0x70000000u); "+old
else:raise SystemExit("Unknown stream locale fault fixture")
assert source.count(old)==1
p.write_text(source.replace(old,new))
PYFAULT
fi
if [[ "${1:-}" == --explore-published* ]]; then
    task_test_source="$task_root/switch-port/tests/test_published.cpp"
fi
if [[ "${1:-}" == --explore-published-*-fault ]]; then
    python3 - "$task_generated/entry_slice.cpp" "$1" <<'PYFAULT'
from pathlib import Path
import sys
p=Path(sys.argv[1]);source=p.read_text();mode=sys.argv[2]
if mode in ("--explore-published-state-fault","--explore-published-tls-state-fault"):
    old="ctx->lr = 0x01686EE8; func_016A0500(ctx); DRAIN_TRAMPOLINE(ctx);"
    address="0x6000005Cu" if mode=="--explore-published-state-fault" else "0xE000430u"
    new="vm_write32("+address+",0u); "+old
elif mode in ("--explore-published-copy-input-fault","--explore-published-copy-output-fault"):
    old="ctx->lr = 0x00F173D4; func_00011720(ctx); DRAIN_TRAMPOLINE(ctx);"
    reg=4 if mode=="--explore-published-copy-input-fault" else 3
    new=f"ctx->gpr[{reg}]=0x70000000u; "+old
elif mode=="--explore-published-tls-consumer-fault":
    old="ctx->lr = 0x01689308; func_016B7F40(ctx); DRAIN_TRAMPOLINE(ctx);"
    new=old+" vm_write32(0xE000430u,0x70000000u);"
elif mode=="--explore-published-capacity-fault":
    old="ctx->lr = 0x00F173A0; func_00179118(ctx); DRAIN_TRAMPOLINE(ctx);"
    new="vm_write32(ctx->gpr[3]+24,0u); "+old
elif mode=="--explore-published-argument-fault":
    old="ctx->lr = 0x016A051C; func_0168ABD0(ctx); DRAIN_TRAMPOLINE(ctx);"
    new="if (vm_read32(0xE000430u)) ctx->gpr[3]=41; "+old
else:
    raise SystemExit("Unknown publication fault fixture")
assert source.count(old)==1
p.write_text(source.replace(old,new))
PYFAULT
fi
if [[ "${1:-}" == --explore-locale* ]]; then
    task_test_source="$task_root/switch-port/tests/test_locale.cpp"
fi
if [[ "${1:-}" == --explore-locale-*-fault ]]; then
    python3 - "$task_generated/entry_slice.cpp" "$1" <<'PYFAULT'
from pathlib import Path
import sys
p=Path(sys.argv[1]); source=p.read_text(); mode=sys.argv[2]
if mode=="--explore-locale-state-fault":
    old="ctx->lr = 0x016891DC; func_01676088(ctx); DRAIN_TRAMPOLINE(ctx);"
    new="vm_write32(0x60000044u, 0u); "+old
elif mode=="--explore-locale-lock-fault":
    old="ctx->lr = 0x01689278; func_0166F990(ctx); DRAIN_TRAMPOLINE(ctx);"
    new=old+" vm_write32(0u, 1u);"
elif mode=="--explore-locale-tls-fault":
    old="ctx->lr = 0x01686E14; func_016896C0(ctx); DRAIN_TRAMPOLINE(ctx);"
    new="vm_write32(0xE000430u, 0x70000000u); "+old
elif mode=="--explore-locale-argument-fault":
    old="ctx->lr = 0x016892DC; func_0168ABD0(ctx); DRAIN_TRAMPOLINE(ctx);"
    new="ctx->gpr[3] = 53; "+old
else:
    raise SystemExit("Unknown locale fault fixture")
assert source.count(old)==1
p.write_text(source.replace(old,new))
PYFAULT
fi
if [[ "${1:-}" == "--explore-allocated" || "${1:-}" == --explore-allocated-*-fault ]]; then
    task_test_source="$task_root/switch-port/tests/test_allocated.cpp"
fi
if [[ "${1:-}" == "--explore-sort-matrix" ]]; then
    task_test_source="$task_root/switch-port/tests/test_sort.cpp"
    python3 - "$task_generated/entry_slice.cpp" <<'PYFAULT'
from pathlib import Path
import sys
p = Path(sys.argv[1]); source = p.read_text()
old = "ctx->lr = 0x00AE5324; func_00AE0BE8(ctx); DRAIN_TRAMPOLINE(ctx);"
assert source.count(old) == 1
# Only seed the outer sort reached by the real tenth constructor. The
# original sorting bodies, comparisons and payload moves remain unchanged.
hook = "if (ctx->gpr[4] - ctx->gpr[3] == 36u * 8u) { ctx->gpr[4] = ds_test_sort_seed((unsigned int)ctx->gpr[3]); ctx->gpr[5] = ds_test_sort_depth; } "
source = source.replace(old, hook + old)
source = 'extern "C" unsigned int ds_test_sort_seed(unsigned int);\nextern "C" unsigned int ds_test_sort_depth;\n' + source
p.write_text(source)
PYFAULT
fi
if [[ "${1:-}" == "--explore-tenth-sort-fault" ]]; then
    python3 - "$task_generated/entry_slice.cpp" <<'PYFAULT'
from pathlib import Path
import sys
p = Path(sys.argv[1]); source = p.read_text()
old = "ctx->lr = 0x00AE0C60; func_00ADF198(ctx); DRAIN_TRAMPOLINE(ctx);"
assert source.count(old) == 1
p.write_text(source.replace(old, "ctx->gpr[4] = 0; " + old))
PYFAULT
fi
if [[ "${1:-}" == "--explore-tenth-state-fault" ]]; then
    python3 - "$task_generated/entry_slice.cpp" <<'PYFAULT'
from pathlib import Path
import sys
p = Path(sys.argv[1]); source = p.read_text()
old = "ctx->lr = 0x0066A390; func_00ADE478(ctx); DRAIN_TRAMPOLINE(ctx);"
assert source.count(old) == 1
p.write_text(source.replace(old, old + " vm_write32(0x1AD8EBCu, 0u);"))
PYFAULT
fi
if [[ "${1:-}" == "--explore-eleventh-state-fault" ]]; then
    python3 - "$task_generated/entry_slice.cpp" <<'PYFAULT'
from pathlib import Path
import sys
p = Path(sys.argv[1]); source = p.read_text()
old = "ctx->lr = 0x005FD708; func_00DBA100(ctx); DRAIN_TRAMPOLINE(ctx);"
assert source.count(old) == 1
p.write_text(source.replace(old, old + " vm_write32(0x1B48C64u, 0u);"))
PYFAULT
fi
if [[ "${1:-}" == "--explore-twelfth-state-fault" || "${1:-}" == "--explore-twelfth-key-fault" || "${1:-}" == "--explore-twelfth-sort-fault" ]]; then
    python3 - "$task_generated/entry_slice.cpp" "$1" <<'PYFAULT'
from pathlib import Path
import sys
p=Path(sys.argv[1]); source=p.read_text()
if sys.argv[2]=="--explore-twelfth-sort-fault":
    old="ctx->lr = 0x00AE0C60; func_00ADF198(ctx); DRAIN_TRAMPOLINE(ctx);"
    replacement="if (ctx->gpr[5] - ctx->gpr[4] == 40u * 8u) ctx->gpr[4] = 0; " + old
else:
    old="ctx->lr = 0x003F4598; func_00ADE478(ctx); DRAIN_TRAMPOLINE(ctx);"
    address="0x1AD8E04u" if sys.argv[2]=="--explore-twelfth-key-fault" else "0x1B4287Cu"
    replacement=old+" vm_write32("+address+", 0u);"
assert source.count(old)==1
p.write_text(source.replace(old,replacement))
PYFAULT
fi
if [[ "${1:-}" == "--explore-runtime-state-fault" || "${1:-}" == "--explore-runtime-lock-fault" || "${1:-}" == "--explore-runtime-argument-fault" ]]; then
    python3 - "$task_generated/entry_slice.cpp" "$1" <<'PYFAULT'
from pathlib import Path
import sys
p=Path(sys.argv[1]); source=p.read_text()
if sys.argv[2]=="--explore-runtime-lock-fault":
    old="ctx->lr = 0x01686D44; func_0166F990(ctx); DRAIN_TRAMPOLINE(ctx);"
    replacement=old+" vm_write32(0u, 1u);"
else:
    old="ctx->lr = 0x016A051C; func_0168ABD0(ctx); DRAIN_TRAMPOLINE(ctx);"
    replacement=("ctx->gpr[3] = 41; " if sys.argv[2]=="--explore-runtime-argument-fault" else "vm_write32(0x1AD6020u, 0u); ")+old
assert source.count(old)==1
p.write_text(source.replace(old,replacement))
PYFAULT
fi
if [[ "${1:-}" == "--explore-allocated-state-fault" || "${1:-}" == "--explore-allocated-native-fault" || "${1:-}" == "--explore-allocated-argument-fault" ]]; then
    python3 - "$task_generated/entry_slice.cpp" "$1" <<'PYFAULT'
from pathlib import Path
import sys
p=Path(sys.argv[1]); source=p.read_text()
if sys.argv[2]=="--explore-allocated-native-fault":
    old="ctx->lr = 0x016A0530; func_01676258(ctx); DRAIN_TRAMPOLINE(ctx);"
    new=old+" vm_write32(0u, 1u);"
elif sys.argv[2]=="--explore-allocated-state-fault":
    old="ctx->lr = 0x01686E14; func_016896C0(ctx); DRAIN_TRAMPOLINE(ctx);"
    new="vm_write32(0x60000018u, 0u); "+old
else:
    old="ctx->lr = 0x016A051C; func_0168ABD0(ctx); DRAIN_TRAMPOLINE(ctx);"
    new="ctx->gpr[3] = 41; "+old
assert source.count(old)==1
p.write_text(source.replace(old,new))
PYFAULT
fi
task_flags=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -pthread)
task_includes=(-I "$task_root/switch-port/include" -I "$task_root/switch-port/entry" -I "$task_generated")
clang -std=c17 "${task_flags[@]}" "${task_includes[@]}" \
    -c "$task_root/switch-port/source/ds_image.c" -o "$task_build/ds_image.o"
clang -std=c17 "${task_flags[@]}" "${task_includes[@]}" \
    -c "$task_root/switch-port/entry/ds_heap.c" -o "$task_build/ds_heap.o"
clang -std=c17 "${task_flags[@]}" "${task_includes[@]}" -fno-strict-aliasing \
    -c "$task_root/switch-port/entry/ds_atomic.c" -o "$task_build/ds_atomic.o"
task_cpp=(-std=c++20 -fwrapv -fno-strict-aliasing -ffp-contract=off)
clang++ "${task_cpp[@]}" "${task_flags[@]}" "${task_includes[@]}" \
    -c "$task_root/switch-port/entry/ds_kernel_mutex.cpp" -o "$task_build/ds_kernel_mutex.o"
clang++ "${task_cpp[@]}" "${task_flags[@]}" "${task_includes[@]}" \
    -c "$task_root/switch-port/entry/ds_rwlock.cpp" -o "$task_build/ds_rwlock.o"
clang++ "${task_cpp[@]}" "${task_flags[@]}" "${task_includes[@]}" \
    -c "$task_root/switch-port/entry/ds_sync.cpp" -o "$task_build/ds_sync.o"
clang++ "${task_cpp[@]}" "${task_flags[@]}" "${task_includes[@]}" \
    -c "$task_root/switch-port/entry/ds_clock.cpp" -o "$task_build/ds_clock.o"
clang++ "${task_cpp[@]}" "${task_flags[@]}" "${task_includes[@]}" \
    -c "$task_root/switch-port/entry/ds_entry.cpp" -o "$task_build/ds_entry.o"
clang++ "${task_cpp[@]}" "${task_flags[@]}" "${task_includes[@]}" \
    -c "$task_test_source" -o "$task_build/test_entry.o"
task_generated_warnings=(-Wno-unused-function -Wno-unused-variable -Wno-unused-label -Wno-unused-parameter -Wno-comment -Wno-parentheses-equality)
clang++ "${task_cpp[@]}" "${task_flags[@]}" "${task_generated_warnings[@]}" "${task_includes[@]}" \
    -c "$task_generated/entry_slice.cpp" -o "$task_build/entry_slice.o"
task_link_extra=()
if [[ "${1:-}" == "--explore-allocated-provider-fault" ]]; then
    clang -std=c17 "${task_flags[@]}" "${task_includes[@]}" \
        -c "$task_root/switch-port/tests/heap_provider_failure.c" -o "$task_build/heap_provider_failure.o"
    task_link_extra=("$task_build/heap_provider_failure.o" -Wl,--wrap=ds_image_add_zero_region)
fi
clang++ "${task_flags[@]}" "$task_build/ds_image.o" "$task_build/ds_heap.o" "$task_build/ds_atomic.o" "$task_build/ds_entry.o" "$task_build/ds_clock.o" "$task_build/ds_sync.o" "$task_build/ds_rwlock.o" "$task_build/ds_kernel_mutex.o" \
    "$task_build/test_entry.o" "$task_build/entry_slice.o" "${task_link_extra[@]}" -o "$task_build/test_entry"
if [[ "${1:-}" == --explore-wide-streams-faults ]]; then
    for task_allocation in {14..25}; do
        for task_fault in size caller stack toc; do
            ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
                "$task_build/test_entry" "$task_root/upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF" "--explore-wide-streams-alloc-$task_allocation-$task_fault-fault" \
                > "/tmp/wide-streams-alloc-$task_allocation-$task_fault-fault.log" 2>&1
            tail -n 1 "/tmp/wide-streams-alloc-$task_allocation-$task_fault-fault.log"
        done
    done
    for task_fault in registry references fill handle buffer cleanup counter callback next-target extra lock virtual-target virtual-argument virtual-stack virtual-toc conversion-pointer native-last conversion-result reorder; do
        ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
            "$task_build/test_entry" "$task_root/upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF" "--explore-wide-streams-$task_fault-fault" \
            > "/tmp/wide-streams-$task_fault-fault.log" 2>&1
        tail -n 1 "/tmp/wide-streams-$task_fault-fault.log"
    done
    exit 0
fi
if [[ "${1:-}" == --explore-narrow-streams-faults ]]; then
    for task_allocation in 9 10 11 12 13; do
        for task_fault in size caller stack toc; do
            ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
                "$task_build/test_entry" "$task_root/upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF" "--explore-narrow-streams-alloc-$task_allocation-$task_fault-fault" \
                > "/tmp/narrow-streams-alloc-$task_allocation-$task_fault-fault.log" 2>&1
            tail -n 1 "/tmp/narrow-streams-alloc-$task_allocation-$task_fault-fault.log"
        done
    done
    for task_fault in registry references buffer tie locale facet lock virtual reorder extra boundary-caller boundary-stack boundary-toc; do
        ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
            "$task_build/test_entry" "$task_root/upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF" "--explore-narrow-streams-$task_fault-fault" \
            > "/tmp/narrow-streams-$task_fault-fault.log" 2>&1
        tail -n 1 "/tmp/narrow-streams-$task_fault-fault.log"
    done
    exit 0
fi
if [[ "${1:-}" == --explore-first-stream-faults ]]; then
    for task_fault in registration fill slot virtual ninth stack lock zero five forty caller toc other-stack descriptor target; do
        ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
            "$task_build/test_entry" "$task_root/upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF" "--explore-first-stream-$task_fault-fault" \
            > "/tmp/first-stream-$task_fault-fault.log" 2>&1
        tail -n 1 "/tmp/first-stream-$task_fault-fault.log"
    done
    exit 0
fi
if [[ "${1:-}" == "--singleton-probe" || "${1:-}" == "--registry-probe" || "${1:-}" == "--registration-probe" || "${1:-}" == "--first-constructor-probe" || "${1:-}" == "--second-constructor-probe" || "${1:-}" == --explore-* ]]; then
    ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
        "$task_build/test_entry" "$task_root/upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF" "$1" | tee "$task_build/latest-probe.log"
    exit 0
fi
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
    "$task_build/test_entry" "$task_root/upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF"

# A local test-only mutation verifies the unsupported-import path. Production
# generator hashes and instruction bodies are never changed or packaged from it.
python3 - "$task_generated/entry_slice.cpp" "$task_build/entry_slice_unknown.cpp" <<'PY'
from pathlib import Path
import sys
source = Path(sys.argv[1]).read_text()
old = "ps3_hle_call(0x744680A2u, ctx)"
assert source.count(old) == 1
Path(sys.argv[2]).write_text(source.replace(old, "ps3_hle_call(0xDEADBEEFu, ctx)"))
PY
clang++ "${task_cpp[@]}" "${task_flags[@]}" "${task_generated_warnings[@]}" "${task_includes[@]}" \
    -c "$task_build/entry_slice_unknown.cpp" -o "$task_build/entry_slice_unknown.o"
clang++ "${task_flags[@]}" "$task_build/ds_image.o" "$task_build/ds_heap.o" "$task_build/ds_atomic.o" "$task_build/ds_entry.o" "$task_build/ds_clock.o" "$task_build/ds_sync.o" "$task_build/ds_rwlock.o" "$task_build/ds_kernel_mutex.o" \
    "$task_build/test_entry.o" "$task_build/entry_slice_unknown.o" -o "$task_build/test_entry_unknown"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
    "$task_build/test_entry_unknown" "$task_root/upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF" --unexpected-import

# Reject a changed syscall explicitly after the two implemented services.
python3 - "$task_generated/entry_slice.cpp" "$task_build/entry_slice_syscall.cpp" <<'PY'
from pathlib import Path
import sys
source = Path(sys.argv[1]).read_text()
old = "        lv2_syscall(ctx);"
assert source.count(old) >= 2
Path(sys.argv[2]).write_text(source.replace(old, "        ctx->gpr[11] = 999; lv2_syscall(ctx);", 1))
PY
clang++ "${task_cpp[@]}" "${task_flags[@]}" "${task_generated_warnings[@]}" "${task_includes[@]}" \
    -c "$task_build/entry_slice_syscall.cpp" -o "$task_build/entry_slice_syscall.o"
clang++ "${task_flags[@]}" "$task_build/ds_image.o" "$task_build/ds_heap.o" "$task_build/ds_atomic.o" "$task_build/ds_entry.o" "$task_build/ds_clock.o" "$task_build/ds_sync.o" "$task_build/ds_rwlock.o" "$task_build/ds_kernel_mutex.o" \
    "$task_build/test_entry.o" "$task_build/entry_slice_syscall.o" -o "$task_build/test_entry_syscall"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
    "$task_build/test_entry_syscall" "$task_root/upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF" --unexpected-syscall

# Exercise null and partially backed syscall output pointers. A marker in the
# valid first word must survive the full-span validation failure.
for task_fault in null tail; do
    python3 - "$task_generated/entry_slice.cpp" "$task_build/entry_slice_$task_fault.cpp" "$task_fault" <<'PY'
from pathlib import Path
import sys
source = Path(sys.argv[1]).read_text()
old = "        lv2_syscall(ctx);"
assert source.count(old) >= 2
replacement = "        ctx->gpr[3] = 0; lv2_syscall(ctx);"
if sys.argv[3] == "tail":
    replacement = "        vm_write32(0x10000FFCu, 0xA5A55A5Au); ctx->gpr[3] = 0x10000FFCu; lv2_syscall(ctx);"
Path(sys.argv[2]).write_text(source.replace(old, replacement, 1))
PY
    clang++ "${task_cpp[@]}" "${task_flags[@]}" "${task_generated_warnings[@]}" "${task_includes[@]}" \
        -c "$task_build/entry_slice_$task_fault.cpp" -o "$task_build/entry_slice_$task_fault.o"
    clang++ "${task_flags[@]}" "$task_build/ds_image.o" "$task_build/ds_heap.o" "$task_build/ds_atomic.o" "$task_build/ds_entry.o" "$task_build/ds_clock.o" "$task_build/ds_sync.o" "$task_build/ds_rwlock.o" "$task_build/ds_kernel_mutex.o" \
        "$task_build/test_entry.o" "$task_build/entry_slice_$task_fault.o" -o "$task_build/test_entry_$task_fault"
    ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
        "$task_build/test_entry_$task_fault" "$task_root/upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF" "--memory-output-$task_fault"
done

# Link-time provider failure fixture. No production clock hooks or fake clock
# implementation enters the Switch build.
cat > "$task_build/clock_failure.cpp" <<'CPP'
#include "ds_clock.h"
extern "C" bool __wrap_ds_clock_now(DsClockSample *sample) { (void)sample; return false; }
CPP
clang++ "${task_cpp[@]}" "${task_flags[@]}" "${task_includes[@]}" \
    -c "$task_build/clock_failure.cpp" -o "$task_build/clock_failure.o"
clang++ "${task_flags[@]}" "$task_build/ds_image.o" "$task_build/ds_heap.o" "$task_build/ds_atomic.o" "$task_build/ds_entry.o" "$task_build/ds_clock.o" "$task_build/ds_sync.o" "$task_build/ds_rwlock.o" "$task_build/ds_kernel_mutex.o" \
    "$task_build/test_entry.o" "$task_build/entry_slice.o" "$task_build/clock_failure.o" \
    -Wl,--wrap=ds_clock_now -o "$task_build/test_clock_failure"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
    "$task_build/test_clock_failure" "$task_root/upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF" --clock-provider-failure

# Constructor sync failures: unsupported wait and a guest fault immediately
# after a real native acquisition. Teardown must release the held native lock.
for task_sync_fault in timeout lock-fault; do
    python3 - "$task_generated/entry_slice.cpp" "$task_build/entry_slice_$task_sync_fault.cpp" "$task_sync_fault" <<'PY'
from pathlib import Path
import sys
source = Path(sys.argv[1]).read_text()
old = "ps3_hle_call(0x1573DC3Fu, ctx)"
assert source.count(old) == 1
replacement = "ctx->gpr[4] = 1; ps3_hle_call(0x1573DC3Fu, ctx)"
if sys.argv[3] == "lock-fault":
    replacement = "ps3_hle_call(0x1573DC3Fu, ctx); vm_write32(0u, 1u)"
Path(sys.argv[2]).write_text(source.replace(old, replacement))
PY
    clang++ "${task_cpp[@]}" "${task_flags[@]}" "${task_generated_warnings[@]}" "${task_includes[@]}" \
        -c "$task_build/entry_slice_$task_sync_fault.cpp" -o "$task_build/entry_slice_$task_sync_fault.o"
    clang++ "${task_flags[@]}" "$task_build/ds_image.o" "$task_build/ds_heap.o" "$task_build/ds_atomic.o" "$task_build/ds_entry.o" "$task_build/ds_clock.o" "$task_build/ds_sync.o" "$task_build/ds_rwlock.o" "$task_build/ds_kernel_mutex.o" \
        "$task_build/test_entry.o" "$task_build/entry_slice_$task_sync_fault.o" -o "$task_build/test_entry_$task_sync_fault"
    ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
        "$task_build/test_entry_$task_sync_fault" "$task_root/upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF" "--constructor-$task_sync_fault"
done

# Only the sys_rwlock_create call is changed in these local fault fixtures.
for task_rw_fault in output attribute shared; do
    python3 - "$task_generated/entry_slice.cpp" "$task_build/entry_slice_rw_$task_rw_fault.cpp" "$task_rw_fault" <<'PY'
from pathlib import Path
import sys
source = Path(sys.argv[1]).read_text()
old = "ctx->gpr[11] = (int64_t)(int32_t)(0x78);\n        lv2_syscall(ctx);"
assert source.count(old) == 1
fault = {"output": "ctx->gpr[3] = 0;", "attribute": "ctx->gpr[4] = 0;", "shared": "vm_write32(ctx->gpr[4] + 4, 0x100u);"}[sys.argv[3]]
Path(sys.argv[2]).write_text(source.replace(old, "ctx->gpr[11] = (int64_t)(int32_t)(0x78);\n        " + fault + " lv2_syscall(ctx);"))
PY
    clang++ "${task_cpp[@]}" "${task_flags[@]}" "${task_generated_warnings[@]}" "${task_includes[@]}" \
        -c "$task_build/entry_slice_rw_$task_rw_fault.cpp" -o "$task_build/entry_slice_rw_$task_rw_fault.o"
    clang++ "${task_flags[@]}" "$task_build/ds_image.o" "$task_build/ds_heap.o" "$task_build/ds_atomic.o" "$task_build/ds_entry.o" "$task_build/ds_clock.o" "$task_build/ds_sync.o" "$task_build/ds_rwlock.o" "$task_build/ds_kernel_mutex.o" \
        "$task_build/test_entry.o" "$task_build/entry_slice_rw_$task_rw_fault.o" -o "$task_build/test_entry_rw_$task_rw_fault"
    ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
        "$task_build/test_entry_rw_$task_rw_fault" "$task_root/upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF" "--rwlock-$task_rw_fault"
done
cat > "$task_build/rwlock_failure.cpp" <<'CPP'
#include "ds_rwlock.h"
extern "C" bool __wrap_ds_rwlock_create(uint32_t *handle) { (void)handle; return false; }
CPP
clang++ "${task_cpp[@]}" "${task_flags[@]}" "${task_includes[@]}" \
    -c "$task_build/rwlock_failure.cpp" -o "$task_build/rwlock_failure.o"
clang++ "${task_flags[@]}" "$task_build/ds_image.o" "$task_build/ds_heap.o" "$task_build/ds_atomic.o" "$task_build/ds_entry.o" "$task_build/ds_clock.o" "$task_build/ds_sync.o" "$task_build/ds_rwlock.o" "$task_build/ds_kernel_mutex.o" \
    "$task_build/test_entry.o" "$task_build/entry_slice.o" "$task_build/rwlock_failure.o" \
    -Wl,--wrap=ds_rwlock_create -o "$task_build/test_rwlock_failure"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
    "$task_build/test_rwlock_failure" "$task_root/upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF" --rwlock-native-failure

# Exercise endian-correct CAS and actual native writer ownership independently
# of the lifted startup path, including malformed ranges and held teardown.
clang -std=c17 "${task_flags[@]}" "${task_includes[@]}" \
    "$task_root/switch-port/tests/test_atomic.c" "$task_build/ds_image.o" "$task_build/ds_heap.o" "$task_build/ds_atomic.o" \
    -o "$task_build/test_atomic"
clang++ "${task_cpp[@]}" "${task_flags[@]}" "${task_includes[@]}" \
    "$task_root/switch-port/tests/test_rwlock_write.cpp" "$task_build/ds_rwlock.o" "$task_build/ds_kernel_mutex.o" \
    -o "$task_build/test_rwlock_write"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$task_build/test_atomic"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$task_build/test_rwlock_write"

# Broader startup failures retain strict boundaries and release acquired locks.
for task_startup_fault in writer wait fallback; do
    python3 - "$task_generated/entry_slice.cpp" "$task_build/entry_slice_startup_$task_startup_fault.cpp" "$task_startup_fault" <<'PY'
from pathlib import Path
import sys
source = Path(sys.argv[1]).read_text()
old = "ctx->gpr[11] = (int64_t)(int32_t)(0x7D);\n        lv2_syscall(ctx);"
replacement = "ctx->gpr[11] = (int64_t)(int32_t)(0x7D);\n        lv2_syscall(ctx); vm_write32(0u, 1u);"
if sys.argv[3] == "wait":
    replacement = "ctx->gpr[11] = (int64_t)(int32_t)(0x7D);\n        ctx->gpr[4] = 1; lv2_syscall(ctx);"
elif sys.argv[3] == "fallback":
    old = "if ((uint32_t)ctx->gpr[3] == 0) { /* boot-patch: NULL heap pool -> ds_heap */"
    replacement = "if (true) { /* test-only forced unsupported fallback */"
assert source.count(old) == 1
Path(sys.argv[2]).write_text(source.replace(old, replacement))
PY
    clang++ "${task_cpp[@]}" "${task_flags[@]}" "${task_generated_warnings[@]}" "${task_includes[@]}" \
        -c "$task_build/entry_slice_startup_$task_startup_fault.cpp" -o "$task_build/entry_slice_startup_$task_startup_fault.o"
    clang++ "${task_flags[@]}" "$task_build/ds_image.o" "$task_build/ds_heap.o" "$task_build/ds_atomic.o" "$task_build/ds_entry.o" "$task_build/ds_clock.o" "$task_build/ds_sync.o" "$task_build/ds_rwlock.o" "$task_build/ds_kernel_mutex.o" \
        "$task_build/test_entry.o" "$task_build/entry_slice_startup_$task_startup_fault.o" -o "$task_build/test_startup_$task_startup_fault"
    ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
        "$task_build/test_startup_$task_startup_fault" "$task_root/upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF" "--startup-$task_startup_fault"
done
cat > "$task_build/atomic_failure.cpp" <<'CPP'
#include "ds_atomic.h"
extern "C" bool __wrap_ds_image_compare_exchange_be32(DsImage *, uint64_t, uint32_t, uint32_t, bool *) { return false; }
CPP
clang++ "${task_cpp[@]}" "${task_flags[@]}" "${task_includes[@]}" \
    -c "$task_build/atomic_failure.cpp" -o "$task_build/atomic_failure.o"
clang++ "${task_flags[@]}" "$task_build/ds_image.o" "$task_build/ds_heap.o" "$task_build/ds_atomic.o" "$task_build/ds_entry.o" "$task_build/ds_clock.o" "$task_build/ds_sync.o" "$task_build/ds_rwlock.o" "$task_build/ds_kernel_mutex.o" \
    "$task_build/test_entry.o" "$task_build/entry_slice.o" "$task_build/atomic_failure.o" \
    -Wl,--wrap=ds_image_compare_exchange_be32 -o "$task_build/test_startup_atomic"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
    "$task_build/test_startup_atomic" "$task_root/upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF" --startup-atomic

# The interface allocator owns the manager lock across the real split. A guest
# fault immediately after acquisition must release that native lock on teardown.
python3 - "$task_generated/entry_slice.cpp" "$task_build/entry_slice_registry_lock.cpp" <<'PY'
from pathlib import Path
import re, sys
source = Path(sys.argv[1]).read_text()
body = re.search(r"^void func_00B27658\(ppu_context\* ctx\) \{\n.*?^\}", source, re.M | re.S)
assert body
old = "ps3_indirect_call(ctx); DRAIN_TRAMPOLINE(ctx);"
assert body[0].count(old) == 2
fault = body[0].replace(old, old + " vm_write32(0u, 1u);", 1)
Path(sys.argv[2]).write_text(source[:body.start()] + fault + source[body.end():])
PY
clang++ "${task_cpp[@]}" "${task_flags[@]}" "${task_generated_warnings[@]}" "${task_includes[@]}" \
    -c "$task_build/entry_slice_registry_lock.cpp" -o "$task_build/entry_slice_registry_lock.o"
clang++ "${task_flags[@]}" "$task_build/ds_image.o" "$task_build/ds_heap.o" "$task_build/ds_atomic.o" "$task_build/ds_entry.o" "$task_build/ds_clock.o" "$task_build/ds_sync.o" "$task_build/ds_rwlock.o" "$task_build/ds_kernel_mutex.o" \
    "$task_build/test_entry.o" "$task_build/entry_slice_registry_lock.o" -o "$task_build/test_registry_lock"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
    "$task_build/test_registry_lock" "$task_root/upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF" --registry-lock-fault

# Only syscall 100 in the newly inspected heavyweight-mutex wrapper is mutated.
# These generated host fixtures never enter the Switch package.
for task_kernel_fault in output attribute shared after; do
    python3 - "$task_generated/entry_slice.cpp" "$task_build/entry_slice_kernel_$task_kernel_fault.cpp" "$task_kernel_fault" <<'PY'
from pathlib import Path
import sys
source = Path(sys.argv[1]).read_text()
old = "ctx->gpr[11] = (int64_t)(int32_t)(0x64);\n        lv2_syscall(ctx);"
assert source.count(old) == 1
fault = {
    "output": "ctx->gpr[3] = 0; lv2_syscall(ctx);",
    "attribute": "ctx->gpr[4] = 0x10000FF0; lv2_syscall(ctx);",
    "shared": "vm_write32(ctx->gpr[4] + 8, 0x100u); lv2_syscall(ctx);",
    "after": "lv2_syscall(ctx); vm_write32(0u, 1u);",
}[sys.argv[3]]
Path(sys.argv[2]).write_text(source.replace(old, "ctx->gpr[11] = (int64_t)(int32_t)(0x64);\n        " + fault))
PY
    clang++ "${task_cpp[@]}" "${task_flags[@]}" "${task_generated_warnings[@]}" "${task_includes[@]}" \
        -c "$task_build/entry_slice_kernel_$task_kernel_fault.cpp" -o "$task_build/entry_slice_kernel_$task_kernel_fault.o"
    clang++ "${task_flags[@]}" "$task_build/ds_image.o" "$task_build/ds_heap.o" "$task_build/ds_atomic.o" "$task_build/ds_entry.o" "$task_build/ds_clock.o" "$task_build/ds_sync.o" "$task_build/ds_rwlock.o" "$task_build/ds_kernel_mutex.o" \
        "$task_build/test_entry.o" "$task_build/entry_slice_kernel_$task_kernel_fault.o" -o "$task_build/test_kernel_$task_kernel_fault"
    ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
        "$task_build/test_kernel_$task_kernel_fault" "$task_root/upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF" "--kernel-$task_kernel_fault"
done
cat > "$task_build/kernel_failure.cpp" <<'CPP'
#include "ds_kernel_mutex.h"
extern "C" bool __wrap_ds_kernel_mutex_create(uint32_t *) { return false; }
CPP
clang++ "${task_cpp[@]}" "${task_flags[@]}" "${task_includes[@]}" \
    -c "$task_build/kernel_failure.cpp" -o "$task_build/kernel_failure.o"
clang++ "${task_flags[@]}" "$task_build/ds_image.o" "$task_build/ds_heap.o" "$task_build/ds_atomic.o" "$task_build/ds_entry.o" "$task_build/ds_clock.o" "$task_build/ds_sync.o" "$task_build/ds_rwlock.o" "$task_build/ds_kernel_mutex.o" \
    "$task_build/test_entry.o" "$task_build/entry_slice.o" "$task_build/kernel_failure.o" \
    -Wl,--wrap=ds_kernel_mutex_create -o "$task_build/test_kernel_provider"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
    "$task_build/test_kernel_provider" "$task_root/upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF" --kernel-provider
