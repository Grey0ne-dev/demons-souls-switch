#!/usr/bin/env python3
"""Execute emitted link/call snippets, including untaken and no-link branches."""
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "upstream/DemonsSoulsRecomp/ps3recomp/tools"))
from ppu_lifter import Instruction, LiftedFunction, PPULifter

lifter = PPULifter()
function = LiftedFunction(start_addr=0x1000, end_addr=0x2000)
cases = [("bctrl", "", True, True, False),
         ("blrl", "", True, True, True),
         ("beqctrl", "cr0", True, True, False),
         ("beqctrl", "cr0", False, True, False),
         ("beqlrl", "cr0", True, True, True),
         ("beqlrl", "cr0", False, True, True),
         ("bctr", "", True, False, False),
         ("beqctr", "cr0", True, False, False),
         ("beqctr", "cr0", False, False, False)]
source = ["""
#include <cstdint>
#include <cstdio>
struct ppu_context { uint64_t lr, ctr; uint8_t crf[8]; uint32_t xer; };
static unsigned calls;
static uint64_t seen_lr, seen_target;
static void ps3_indirect_call(ppu_context *ctx) {
    ++calls; seen_lr=ctx->lr; seen_target=ctx->ctr;
}
#define DRAIN_TRAMPOLINE(ctx) ((void)(ctx))
"""]
for i, (mnemonic, operands, taken, link, via_lr) in enumerate(cases):
    statement = lifter._translate_op(Instruction(0x1230, 0, mnemonic, operands), function)
    source.append(f"static void case_{i}(ppu_context *ctx) {{ {statement} }}")
source.append("int main() { unsigned checks=0, failures=0;")
for i, (_, _, taken, link, via_lr) in enumerate(cases):
    expected_lr = 0x1234 if taken and link else 0x1020304
    expected_target = 0x1020304 if via_lr else 0x5060708
    source.append(f"""
    {{ ppu_context ctx={{}}; ctx.lr=0x1020304; ctx.ctr=0x5060708;
      ctx.crf[0]={2 if taken else 0}; calls=0; seen_lr=seen_target=0;
      case_{i}(&ctx); ++checks;
      bool ok=ctx.lr=={expected_lr} && calls=={int(taken)};
      if (calls) ok &= seen_lr=={expected_lr} && seen_target=={expected_target};
      if (!ok) {{ ++failures; std::fprintf(stderr,"Link case {i} failed\\n"); }} }}
    """)
source.append('std::printf("Lifter indirect-link execution: %u checks, %u failures\\n",checks,failures); return failures?1:0; }')
with tempfile.TemporaryDirectory(prefix="ds-lifter-link-") as directory:
    path = Path(directory)
    (path / "links.cpp").write_text("\n".join(source))
    subprocess.run(["clang++", "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                    str(path / "links.cpp"), "-o", str(path / "links")], check=True)
    subprocess.run([str(path / "links")], check=True)
