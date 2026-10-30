# First locale facet and cleanup registration — v0.24

`ds_entry_run_first_facet` serves the original 24-byte facet and 8-byte cleanup
node allocations. It runs the original locale guard constructor, copies the
84-byte locale snapshot, constructs its C strings, restores locale state and
destroys those temporary strings. It publishes the facet in the original cache,
increments its reference count, registers the cleanup callback and links the
real cleanup node. It then releases all guards and the temporary locale handle.
The original stream constructor and constructor thirteen remain incomplete.

Stop 35 is before invoking virtual target 009E96A0 through OPD 01970348:
799 entries / 13,238 checked accesses, r3=600000D0, r4=32, r5=1,
r6=7F7F7F7F7F7F7F7F, LR=01688280, SP=0FEFFA90, TOC=019B5040.
The original bctrl at 0168827C is checked, as are executable target backing,
the descriptor and TOC. The target is not selected, invoked or counted. No
general non-main-TOC dispatch rule was widened; original descriptor loading
performs the transition back to the main TOC at this boundary.

Six reviewed complete bodies are added: 0168A6B0, 0167AED0, 01668208,
01668E50, 016888D8 and 016884C0. All 213 selected bodies are hash-pinned;
original chunks remain unchanged. Indirect markers remain 63 in 28 bodies.
No host string, locale or facet implementation substitutes for original code.

Eight runtime requests are accepted, each with exact size/caller/stack/TOC/
sequence checks: 40/4/52/40/4/4/24/8. The facet at 600000D0 has vtable
01872C50, reference count one and original table pointers 0176E82A/0176EFF4/
0176F1F6. Its cache is 01B50430 (the address loaded through runtime TOC+4C44).
The neighboring word 01CE6728 checked by v0.23 is unrelated to this facet cache;
its earlier description as a default-facet pointer was inaccurate.

Cleanup head 01CE6720 points to 600000F0, whose next link is null and whose
facet pointer is 600000D0. Original atexit registration reduces free slots from
79 to 78 and stores callback OPD 019AC6D0 at 01CE58E4; the older callback at
01CE58E8 remains intact. Registered cleanup/destructor callbacks are data at
this checkpoint and have not been invoked.

The locale object remains at 60000040 with name C, size one, capacity 15 and
flags 63. Its reference count is now five after the original temporary handle
release. All 256 rounded heap bytes are validated, along with the prior
metadata, stream/base state, 21 copied locale words and defined fields of both
destroyed temporary strings. Uninitialized stack padding is not assumed zero.

Requested heap bytes total 176, live-rounded/peak bytes 256, backed bytes 65,536
in the same RW/non-X arena. Total guest backing stays 31,687,456 bytes under
the 50,331,648-byte cap. There are 14 CRT mutexes, fourteen thread-ID imports
and 25/25 native lock acquisitions/releases. No native ownership is held at
stop. Native registries and heap metadata clear; image destruction releases
all backing. Replay is refused; fresh repetitions recreate ownership cleanly.

Host integration checks cover the full state/ABI/accounting and all twelve
earlier metadata/runtime checkpoint APIs. Nine fault fixtures corrupt facet
references, cleanup links, copied snapshot data, allocation sizes, virtual OPD
TOC/target, comparison input backing and memory under recursive wrapper ownership.
Each failure must release all native resources and image backing.

The original comparator passes 16,384 standalone oracle/ABI/memory checks:
sixteen lengths from zero through 256, every pair of alignments modulo eight,
equal strings, prefixes and unsigned-byte differences through FF. An initial
host-test mask was mistyped as seven 7F bytes, causing 34 failures. The corrected
harness uses the exact eight-byte mask verified against the ELF. No production
or original comparator code changed. Both failed evidence and correction record
are retained under device-first-facet-v0240/.

Versioned evidence and an independent device validator are prepared there.
All 17,145 sanitizer checks pass: 589 full regressions, 127 integration checks
including twelve older checkpoints, 16,384 comparator checks and nine five-check
fault cases. Nine compiled lifter checks pass. The strict Switch build and ZIP
CRC/content checks pass. The clean annotated first-facet exploration source and
executable were restored after fault testing and passed again; repeat checks
are not counted twice.

The sealed candidate is 506,053 bytes, SHA256
`1b9aaee6378998e1ec1ab66e3643d95ec8e62ad407c264b11e96dfc7d47e61fc`.
v0.24 is installed through DBI MTP with exact full readback verified at
2026-10-26 09:40 UTC. The prior installed binary and log match verified v0.23
exactly and are preserved. Sealed source/evidence hashes remain intact. MTP is
released. v0.24 passed twice on the actual Switch, including fresh reload.
All 3,591 independent checks pass without mismatches: both full traces and ABI,
eight allocations, facet/cache/cleanup-node state, balanced native ownership,
metadata cleanup and zero guest backing at exit. The clock advances 7,017,434
microseconds. Post-run binary matches the sealed candidate; all sealed source
and evidence hashes remain intact. A separate read-only validator review found
no material contradictions. Evidence: loader.log, device-result.json and
postrun-verification.json. No further v0.24 device action is pending.

Next: review the virtual-method invocation and
remaining stream construction. The next 009E96A0 body has been inspected: it
returns r4 unchanged, but is deliberately unselected at this checkpoint.
The registered facet destructor, later stream registration, further allocations,
growth/free/exception/memmove paths and gameplay remain unimplemented.
