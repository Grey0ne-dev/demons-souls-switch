# Remaining narrow-stream initialization — v0.26 source audit

The scope is completion of original `01686CE8`, after the verified v0.25
checkpoint. The four narrow streams return and register in slots one through
four. Stop before wide initializer `0168BC68`; constructor thirteen remains
incomplete. No wide-stream code, destructor or stream I/O is executed.
Hardware and installation results are recorded separately.

All 215 selected original bodies, 63 indirect markers in 28 bodies and both
cross-TOC thunks retain their v0.25 hashes. No new function body or native service
is selected. A mode-specific checkpoint precedes the existing unsupported wide
initializer. Existing checkpoint APIs preserve their earlier exact boundaries.

The allocation bridge accepts exactly these additional contexts, in order,
with TOC `019D3BAC` and sixteen-byte alignment:

| Request | Size | Return PC | SP | Guest address | Receiver |
| --- | --- | --- | --- | --- | --- |
| 9 | 4 | `01685FEC` | `0FEFFA00` | `60000100` | `01CE662C` |
| 10 | 40 | `016A051C` | `0FEFFB60` | `60000110` | `01CE6644` |
| 11 | 4 | `0168704C` | `0FEFFBF0` | `60000140` | `01CE6678` |
| 12 | 4 | `01685FEC` | `0FEFFA00` | `60000150` | `01CE66D8` |
| 13 | 4 | `01685FEC` | `0FEFFB60` | `60000160` | `01CE6710` |

The allocator consumes the size in r3; its sequence, caller, stack and runtime
TOC are checked before function counting and before allocation. No fourteenth
allocation is accepted. Original `li` and `bl` words are checked against the
original ELF by `switch-port/tests/audit_narrow_streams.py`. Its source audit
also checks the final inline `bctrl`, the TOC restoration and the caller's
relative branch to `0168BC68`. The SPs follow from the original 0x1A0 narrow,
0x160 stream, 0x90 locale-base and 0x90 mutex-helper frames.

Repeated virtual calls at return PC `01688280` retain exact OPD `01970348`,
target `009E96A0`, facet `600000D0`, argument 32 and SP `0FEFFA90` checks.
They occur after allocations 8, 9 and 12. The final inline virtual call at
return PC `016872A0` has SP `0FEFFBF0` and thirteen allocations. Both paths
require executable backing, the actual main TOC loaded by the descriptor and
no held native lock. Original code restores runtime TOC. No general non-main
TOC dispatch rule is widened; no virtual result is synthesized.

The original registry occupies `[01CE6488,01CE64E8)`. Its reference bytes at
indices one through four equal one; the corresponding pointers are `01CE65D0`,
`01CE6608`, `01CE66B4` and `01CE66EC`. Unused slots and padding remain zero.
Each stream stores its original vtable, index, space fill, buffer and tie fields.
The third stream's flags become `0203`; the others retain `0201`. All four
base handles reuse core locale `60000040`, whose references rise to nine.
The existing facet/cache/cleanup node are reused, with facet references one.
The new 40-byte object contains the fifteenth CRT mutex at `60000120`.

The independent model reconstructs every word of the 0x60-byte registry,
0x238-byte narrow aggregate and 0x170-byte rounded heap from the original
initializer's stores and ELF metadata tables: 258 word comparisons. It does
not read the production expected-state header when computing these values.
The header is checked against the model; its trace suffix comes from reviewed
clean execution, with the complete prior 810-entry trace required unchanged.
Four original virtual calls and four registrations execute in total.

Thirteen allocations total 232 requested / 368 rounded live bytes in the same
65,536-byte RW, non-executable arena. Total guest backing remains 31,687,456
bytes within the 50,331,648-byte cap. There are fifteen CRT mutexes, thirty-one
thread-ID imports and 42/42 guest lock/unlock calls, with no ownership held at
stop. Teardown clears all native/heap metadata; image destruction releases all
backing. Stop37 has 966 entries / 14,378 checked accesses and exact ABI:
r3=`01C5BE41`, r4=0, r5=0, r6=`01CE6658`, LR=`015165E8`, SP=`0FEFFD90`,
TOC=`019D3BAC`. Wide initializer entry is neither counted nor executed. Temporary facet locale
snapshots and destroyed strings lived on the returned initializer stack; their
old stack bytes are no longer validated or presented as current state. The
existing facet, locale, cleanup registrations and all live metadata remain checked.

Thirty-three fault fixtures cover each new allocation's size/caller/stack/TOC,
a valid context in the wrong sequence, a fourteenth request, corrupted registry
padding/reference bytes/buffer/tie/locale/cache, final virtual target corruption,
wrong checkpoint caller/stack/TOC, and a guest memory fault after the second real
registration lock acquisition. Test hooks exist only in host fixture output.
Clean exploration is regenerated afterwards and must equal production output.

The next initializer `0168BC68` builds wide aggregates using `0168E548`.
Inspection identified TLS getters `0166CA50` / `0166FE88` and wide virtual
`00B17510`, which reaches cross-TOC thunk `00010C50` and conversion `0166C0E8`.
These are inspected dependencies, not selected or executed coverage. They need
separate character-conversion and allocation audits. Completing the narrow
initializer does not establish complete constructor thirteen, a game menu,
rendering, audio, scheduling, input or gameplay.
