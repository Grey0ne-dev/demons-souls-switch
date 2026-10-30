# First narrow stream construction and registration — v0.25

The new `ds_entry_run_first_stream` checkpoint invokes the original facet
virtual method `009E96A0` and registers the first narrow stream using original
`01684D70`. The method returns the space fill character (32). Registration
publishes stream `01CE65D0` in slot one at `01CE6498` and increments registry
byte `01CE6489` to one. The complete 0x60-byte registry span is validated,
including unused slots and padding. The original narrow-stream constructor
returns; constructor thirteen remains incomplete. The second narrow stream
has begun and stops before its four-byte locale-handle allocation.

Stop36 precedes entry/counting of allocator `0168ABD0` for request nine.
There are 810 original function entries and 13,312 checked guest accesses.
Exact boundary: r3=4,r4=01CE6558,r5=1,r6=7F7F7F7F7F7F7F7F,
LR 01685FEC,SP 0FEFFA00,TOC 019D3BAC. The original virtual call uses checked
callsite 0168827C/return 01688280,OPD 01970348,target 009E96A0 and main
TOC 019B5040. Original code restores runtime TOC afterwards. No general
non-main-TOC dispatch rule is widened.

Two complete original bodies are added, bringing production to 215 pinned
bodies. All prior bodies and 63 original indirect sites in 28 bodies are
unchanged. No host stream, locale or virtual-return substitute is added.
Eight prior allocations remain unchanged: 40/4/52/40/4/4/24/8, 176 requested
bytes, 256 rounded live bytes, 65,536 backed bytes. Total guest backing remains
31,687,456 bytes within the 50,331,648 cap. Locale references remain five;
facet/cache/cleanup-node and prior metadata state are preserved. There are
14 CRT mutexes, 15 thread-ID imports and 26/26 guest lock/unlock calls.
No native lock is held at stop. Native registries and heap metadata clear;
image destruction releases all backing. In-place replay remains rejected.

Clean integration passes 158 ASan/UBSan/LSan checks, including fresh runs
and all thirteen prior metadata/runtime checkpoint APIs. Fifteen fault cases
pass five checks each: registration index, fill character, registry pointer,
virtual return, next sizes0/5/8/40, two wrong stacks, wrong returnPC, wrongTOC,
virtual descriptor/target and memory fault after the real registration lock.
Every failure rejects success and verifies native/heap metadata cleanup and
zero backing after destruction. Fault hooks exist only in generated host test
fixtures; final exploration output is regenerated clean and matches production.
The fault test runner now generates before modifying its output and supports
one compiled fixture for all 15 cases. Original production bodies stay unchanged.
Nine compiled lifter tests and the strict devkitA64/libnx cross-build pass.
All 589 full regression checks pass, for 822 total ASan/UBSan/LSan checks
including clean integration and faults. All test and build processes completed.

Candidate/package: switch-port/demons-souls-loader.nro and
switch-port/demons-souls-switch-startup.zip. Exact size 506,053 bytes;
SHA256 4c5506e323deb47a636571adacf0d3b9f95ecdc0cc4c22840a6d78f69d00cd6c.
Package CRC, manifest hashes and staged NRO equality were verified.
The installed v0.24 binary and device log were preserved and both exactly match
the verified v0.24 evidence.

v0.25 was installed via DBI MTP on 2026-10-28 at 18:12 UTC, with exact
506,053-byte readback verified. It passed twice on the actual Switch, including
fresh reload: all 3,656 independent device checks pass with zero mismatches.
Both complete 810-entry traces, exact ABI, unchanged eight allocations,
stream/registry/facet state, 26/26 locks and complete cleanup match expectations.
The hardware clock advances 6,220,263 microseconds. Post-run binary matches the
sealed candidate; sealed source and evidence hashes remain intact. MTP is
released. No further v0.25 device action is pending. Evidence:
device-first-stream-v0250/loader.log, device-result.json and
postrun-verification.json. The original build manifest retains its historical
pre-device state; the separate device/post-run receipts establish hardware success.

Next code milestone: audit and accept the remaining five narrow-stream
allocation contexts described in FIRST_STREAM_AUDIT.md. Complete 01686CE8,
then separately audit wide-stream initializer 0168BC68 and its dependencies.
Constructor thirteen does not finish when the narrow initializer returns.
Stream I/O, files, graphics, audio, scheduling, input and gameplay remain absent.
