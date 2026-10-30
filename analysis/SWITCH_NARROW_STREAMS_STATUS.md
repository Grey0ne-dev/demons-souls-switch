# Narrow streams complete — v0.26

`ds_entry_run_narrow_streams` completes original initializer 01686CE8. Four
narrow streams construct, invoke the original facet virtual and register in
slots one through four. The new boundary is before wide initializer 0168BC68:
966 original function entries / 14,378 checked guest accesses, 12 completed
constructors. Constructor thirteen is still in progress.

Five added allocations are accepted with exact sequence/size/caller/SP/TOC
checks. Thirteen requests total 232 requested / 368 rounded live bytes in the
existing 65,536-byte RW, non-executable arena. Total guest backing remains
31,687,456 bytes under the 50,331,648 cap. Core locale references rise to nine;
facet/cache/cleanup state is reused. Fifteen CRT mutexes and 31 thread-ID calls
run with 42/42 locks/unlocks and no held ownership at stop. Teardown clears
native/heap metadata; image destruction frees all backing.

All 215 original pinned bodies and 63 indirect markers in 28 bodies remain
unchanged. There is no new native service or host stream substitute. The
fourth virtual call uses the existing original inline callsite and strict
OPD/code/TOC/argument/stack/allocation-count checks. Temporary locale snapshots
on the returned initializer's stack are no longer treated as live objects.
The complete registry, narrow aggregate and rounded heap match a separate
original-source/ELF model: 258 words, plus nine original instruction checks.

Clean integration passes 322 ASan/UBSan/LSan checks, including fresh runs and
all 14 earlier metadata/runtime APIs. Thirty-three six-check fault cases pass,
covering every added allocation context, sequence/cap rejection, corrupted live
state, virtual target rejection and a memory fault under a held registration
lock. The fault fixture was replaced by clean generation and retested; its
source matches production. Nine compiled lifter cases and strict Switch build
pass. All 589 full regression checks also pass: 1,109 sanitizer checks in total.
All test/build jobs completed; no processes remain pending.
The first regression shell was interrupted when its runner was edited; the
completed stable rerun is the evidence used for sealing. That failed run and
its explanation are preserved separately.

Candidate size 514,245 bytes; SHA256
9ae63541f909d998bb0a8cc294c3e41a7bbc8a84eb9a3ed29efb9341df6627ab.
The verified v0.25 installed loader and device log have been preserved exactly.
v0.26 was installed via DBI MTP on 2026-10-28 at 18:34 UTC, with exact
514,245-byte readback verified. It passed twice on the actual Switch, including
fresh reload: 4,308 independent device checks, zero mismatches. Both complete
966-entry traces, 14,378 accesses, exact ABI, all four registrations, thirteen
allocations, 42/42 locks, metadata cleanup and zero final backing match.
The hardware clock advances 7,478,061 microseconds. Post-run installed binary
matches the sealed candidate; all sealed sources, snapshots and evidence hashes
remain intact. MTP is released. No further v0.26 device action is pending. Evidence directory:
device-narrow-streams-v0260/. Audit: NARROW_STREAMS_AUDIT.md.

Next: separately audit wide initializer 0168BC68, stream constructor 0168E548,
wide facet/conversion paths and their allocation contexts. Wide initialization,
registered destruction, stream I/O, graphics, audio, scheduling, input and gameplay remain unimplemented.
