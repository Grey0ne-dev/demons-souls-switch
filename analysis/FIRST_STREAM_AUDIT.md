# First stream construction and registration audit — v0.25

This source and original-ELF audit covers the bounded continuation after the
verified v0.24 first-facet checkpoint. It records expected behavior and rejection
checks. Completed build, test, transfer and hardware results are tracked in
SWITCH_FIRST_STREAM_STATUS.md and device-first-stream-v0250/. This audit alone
does not establish a hardware result. The v0.24 evidence remains unchanged.

The intended scope is to invoke the first facet virtual method, finish and
register the first narrow stream, and stop before the ninth runtime allocation.
Twelve constructors have returned. Constructor thirteen and the second narrow
stream remain incomplete at this boundary.

## Reviewed original bodies

Hashes use the same complete-function extraction as
`switch-port/entry/generate_entry.py`: from `void func_NAME(ppu_context* ctx) {`
through its closing brace, without a trailing newline. Both hashes were
independently recomputed from the unchanged upstream chunks.

| Body | Source chunk | Original range, end exclusive | OPD / TOC | SHA256 |
|---|---|---|---|---|
| `009E96A0` | `ppu_recomp_005.cpp` | `009E96A0–009E96A8` | `01970348` / `019B5040` | `604214d12ee81dc15fea105e242774d9283e0d50b016758be4fcec991660ead5` |
| `01684D70` | `ppu_recomp_011.cpp` | `01684D70–01684E20` | `019AC5E0` / `019D3BAC` | `692949f143532127d12e33a84e13058c73b18b11d1a73dade806122de51e7542` |

`009E96A0` copies r4 to r3 and returns. It has no calls, memory accesses, stack
frame or allocation. Original words are `7C832378` at `009E96A0` and `4E800020`
at `009E96A4`.

`01684D70` has exactly two direct dependencies: existing complete bodies
`0166F990` and `0166F910`, which acquire and release CRT lock index two. It has
no indirect calls or allocations. It chooses the first available or matching
stream registry slot from indices one through seven, stores that slot in the
stream's word at offset four, publishes the stream pointer and increments the
slot's byte counter. Its own frame is 0x80 bytes. This adds no native service or
host implementation of stream behavior.

## Exact virtual invocation

The original ELF SHA256 is
`90eaa5e9e48b4874292badc311ccea253e6a622ff188bfbdbe76e6f6821269d8`.
The audited words were read through its PT_LOAD address mappings.

The first stream is `01CE65D0`, at narrow aggregate `01CE64E8 + E8`. Its cached
facet is `600000D0`, with vtable `01872C50`. The vtable entry at offset `18`
selects OPD `01970348`. Original ELF words at `01970348` and `0197034C` are
`009E96A0` and `019B5040`, respectively.

The original `bctrl` is at `0168827C`, word `4E800421`, with return PC
`01688280`. At invocation, r3 is `600000D0`, r4 is 32, SP is `0FEFFA90`, and
the descriptor has loaded TOC `019B5040`. The target returns r3=32. The original
instruction at `01688280`, word `E8410028`, restores TOC from SP+28 to
`019D3BAC`. Subsequent original code stores byte `20` at stream+30,
`01CE6600`, and calls `01684D70` with return PC `01688448`. Its incoming SP is
`0FEFFA90`; its own frame lowers SP to `0FEFFA10`.

The bounded dispatcher must continue to check this exact callsite, OPD, target,
TOC and ABI. This audit supplies no authorization for a general non-main-TOC
dispatch expansion. Registered facet cleanup and destructor callbacks remain
uninvoked.

## Entire 0x60-byte registry span

Original ELF word `[019D3BAC + 4C04]`, at `019D87B0`, is `01CE6488`. The full
registry validation span is `[01CE6488, 01CE64E8)`, exactly 0x60 bytes and ending
immediately before the narrow aggregate. It is zero at the v0.24 boundary.

The first registration selects slot one. Only these bytes in that span change:

| Location | Expected result |
|---|---|
| Byte `01CE6489`, registry+1 | `01` |
| Word `01CE6498`, registry+C+4*1 | `01CE65D0` |

Every other byte in the 0x60-byte span remains zero. A big-endian word read at
`01CE6488` therefore yields `00010000`. The matching stream field is word
`01CE65D4` = 1. Checks should cover the entire span, including unused slots and
padding, as well as stream byte `01CE6600` = `20`.

There is no intervening allocation. The existing eight requests, 176 requested
bytes, 256 rounded bytes, facet/cache/cleanup state and core locale references
remain unchanged. The registration's lock and unlock are balanced; no native
lock is retained. The next setup writes `018C1B1C` to `01CE6604` and
`018C1B30` to `01CE6608` before entering the second stream constructor.

## Ninth allocation: exact boundary and rejection plan

After the first stream returns to `01686FFC`, the outer initializer calls
`01688138` again with return PC `01687028`, r3=`01CE6608`, r4=`01CE6558`, and
r5=1. This second stream constructor calls `01685FC8`, which requests its
four-byte locale handle.

| Original PC | ELF word | Instruction |
|---|---|---|
| `01685FD8` | `38600004` | `li r3,4` |
| `01685FE8` | `48004BE9` | `bl 0168ABD0` |

The allocator's return PC is `01685FEC`. Its SP is derived from the outer
initializer's `0FEFFBF0`, the second stream frame of 0x160 bytes, and the base
initializer frame of 0x90 bytes: `0FEFFA00`. TOC is `019D3BAC`, r3=4,
r4=`01CE6558`, and r5=1.

The new checkpoint must stop before entering/counting `0168ABD0` for this ninth
request and before invoking malloc. Exactly eight allocations must still be
live. The ninth request is identified by sequence index eight plus its size,
return PC, SP and TOC; it is not accepted merely because its size is four.

The rejection plan is to mutate the ninth request independently and require
`DS_ENTRY_ABI_MISMATCH` before allocation for any wrong size, return PC, SP or
TOC. Representative wrong sizes are 0, 5, 8 and 40; representative wrong stacks
are `0FEFFA90` and `0FEFFB60`, which belong to other reviewed contexts. A wrong
sequence must also fail. These are planned checks, not results reported by this
audit. Every rejection must preserve eight allocation calls and validate native
ownership, heap metadata cleanup and zero image backing after destruction.
Earlier checkpoint APIs must retain their own exact stops and counts.

## Continuation after the bounded checkpoint

On the reviewed successful path, completing the rest of `01686CE8` requires
five more allocations. They all use TOC `019D3BAC`. Addresses below follow from
the existing arena layout, 16-byte rounding, and no intervening frees.

| Size | Return PC | Allocator SP | Expected guest address | Field receiving the pointer |
|---|---|---|---|---|
| 4 | `01685FEC` | `0FEFFA00` | `60000100` | `01CE662C`, second stream+24 |
| 40 | `016A051C` | `0FEFFB60` | `60000110` | `01CE6644`, aggregate+15C |
| 4 | `0168704C` | `0FEFFBF0` | `60000140` | `01CE6678`, aggregate+190 |
| 4 | `01685FEC` | `0FEFFA00` | `60000150` | `01CE66D8`, third stream+24 |
| 4 | `01685FEC` | `0FEFFB60` | `60000160` | `01CE6710`, final stream+24 |

The 40-byte request is the existing `016A0500` helper: ELF `016A0510` is
`38600028` (`li r3,40`), and `016A0518` is `4BFEA6B9` (`bl 0168ABD0`). The
direct four-byte request has ELF words `38600004` at `01687044` and `48003B89`
at `01687048`, returning to `0168704C`.

Completing this narrow initializer predicts 13 live blocks, 232 requested
bytes and 368 rounded bytes. The four new four-byte handles retain the published
core locale `60000040`, predicting its references rise from five to nine.
The additional 40-byte object contains its CRT mutex at `60000120`. The cached
facet `600000D0` and existing cleanup node `600000F0` are reused; the reviewed
path does not require another facet or cleanup-node allocation. Later narrow
streams need the same virtual target and registration body. The final inline
virtual call is original `bctrl` at `0168729C`, word `4E800421`, returning to
`016872A0` with SP `0FEFFBF0`. These are continuation expectations, not executed
v0.25 coverage.

Finishing `01686CE8` does not finish constructor thirteen. Its caller
`01516590` next calls `0168BC68`, returning to `015165E8`. That unselected
initializer builds the wide aggregates and calls unselected `0168E548` four
times, with return PCs `0168BFD8`, `0168C004`, `0168C030` and `0168C064`.
Wide-stream construction and its further dependencies need a separate audit.
Growth, free, new-handler, exception and memmove paths, registered destruction
and gameplay remain beyond this bounded continuation.
