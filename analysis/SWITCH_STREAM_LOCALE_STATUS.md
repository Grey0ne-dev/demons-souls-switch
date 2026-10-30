# Stream locale/base checkpoint — v0.23

`ds_entry_run_stream_locale` extends constructor thirteen through another
40-byte runtime mutex and two four-byte locale handles. The original getter
reuses the actual published TLS locale; its reference count reaches six.
The original stream/base initializer sets state and exception mask to zero,
format flags to 0x201, precision to six and its tied-buffer pointer. It assigns
the first facet ID and performs the original facet lookup. That lookup returns
null, so execution stops before the first 24-byte facet allocation.
The stream constructor and constructor thirteen have not returned.

Four complete reviewed original bodies are added: 01688138 (253 lines),
01685FC8 (44), 01685248 (877) and 01688538 (51). All 207 selected bodies are
hash-pinned; original source chunks remain unchanged. The new stream body adds
two original bctrl markers, bringing the total to 63 in 28 bodies. Neither new
indirect site executes on this accepted path. No native dispatch target or HLE
service has been added. Unsupported error, exception, growth, free, memmove and
later virtual-call paths remain explicit boundaries.

All six accepted runtime requests check exact size, return PC, caller stack,
runtime TOC and sequence before operator-new entry is counted. The native
malloc bridge independently checks size, sequence, TOC and its operator-new
return PC. Requests 40/4/52/40/4/4 return guest offsets 60000000, 60000030,
60000040, 60000080, 600000B0 and 600000C0. Requested bytes total 144; live
rounded and peak bytes are 208. All share the existing 65,536-byte RW/non-X
arena, with total guest backing unchanged at 31,687,456 bytes under its
50,331,648-byte cap. The next request is not allocated.

Stop 34 precedes 0168ABD0 after 747 entries / 12,849 checked accesses:
r3=24, r4=1, r5=0, r6=01CE6564, LR=01688380, SP=0FEFFA90, TOC=019D3BAC.
Twelve static constructors have completed. All prior metadata, all 208 rounded
heap bytes and both locale/stream aggregate ranges are checked. The shared
validator checks the exact 50-entry suffix after v0.22's 697-entry prefix.

The fourteenth CRT mutex lives at 60000090, within the fourth allocation.
The stream aggregate's locale handle at 600000B0 and its base handle at
600000C0 both contain 60000040. Stream/base state begins at 01CE65D0; the
outer stream locale aggregate begins at 01CE6558. Facet ID 01CE6734 and
counter 01CE6724 are both one; neighboring word 01CE6728 remains zero. The
actual default-facet cache at 01B50430 was identified in the v0.24 audit.
The locale name remains `C`, size 1 / capacity 15, flags 63.

There are ten thread-ID imports, 23 native acquisitions / 22 guest releases,
and one held native lock at stop. Nested runtime-wrapper recursion executes
through original code without changing the nonrecursive native mutex adapter.
Cleanup releases held native ownership and clears every native registry and
heap metadata record; image destruction releases all guest backing. The old
v0.22 boundary still stops before the fourth allocation, preserving its 697
entries / 12,492 accesses and zero held native locks.

The integration tests check exact ABI/state/accounting, complete heap contents,
fresh repetitions, replay refusal and v0.22 preservation. Failure fixtures cover
corrupt final locale references, a corrupt stream handle, wrong fourth/fifth/
sixth request sizes, wrong caller stack, a checked fault after actual fourteenth
mutex creation and a non-null TLS pointer consumed by original code. Every
failure must clear native ownership and image backing. ASan, UBSan and leak
detection remain enabled. Full regression and earlier checkpoint tests run
separately. Device evidence and validation scripts are under
`device-stream-locale-v0230/`; hardware results are recorded below.

All 791 sanitizer checks pass: 589 full regressions, 84 stream/base tests,
78 publication/older-checkpoint checks and eight five-check fault cases.
Nine compiled lifter checks pass. The strict Switch build and ZIP CRC/content
checks pass. The clean stream exploration source and executable are restored
after fault testing; the actual stop NID is 350D454E (the last thread-ID import
in original wrapper recursion). No second native acquisition is fabricated.

The sealed candidate is 497,861 bytes, SHA256
`5d513cc6632739b449f3180506c04e878d0738a5eb8faa451fb13487f8449fc7`.
It is installed through DBI MTP with exact full readback verified, at
2026-10-26 09:03 UTC. Prior installed binary and log exactly match verified
v0.22 and are preserved. Sealed source/evidence hashes remain intact; MTP is
released. v0.23 passed twice on the actual Switch, including fresh reload.
All 3,353 independent checks pass with no mismatches: both complete traces,
exact ABI and final service NID, six allocations and heap accounting, both real
mutexes, stream/base locale state, held-lock cleanup and final zero guest backing.
The clock advances 3,533,977 microseconds. Post-run installed binary matches the
sealed candidate; sealed source and evidence hashes remain intact. Evidence:
loader.log, device-result.json and postrun-verification.json. No further v0.23
device action is pending.

Next frontier: audit the 24-byte facet object's constructor 0168A6B0 and its
dependencies before serving that allocation. Its later registration, destruction
and virtual method paths also need review. Stream I/O, file access, graphics, audio,
guest scheduling and gameplay remain unimplemented.
