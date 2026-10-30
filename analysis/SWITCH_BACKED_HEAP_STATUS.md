# Explicitly backed guest heap — v0.20

The runtime now serves the actual constructor-thirteen operator-new requests:
40 bytes at0x60000000 and4 bytes at0x60000030. It executes658 entries and12,247
checked accesses, then stops before unselected locale consumer0x016896C0.
Twelve constructors have returned; thirteen remains incomplete. Stop enum30.
ABI:r3=60000030,r4=0176F780,r5=0,r6=01AD8EE0,LR=01686E14,SP=0FEFFBF0,
TOC=019D3BAC. The original caller publishes60000000 at01CE64EC and initializes
the thirteenth CRT mutex at60000010. Native locks remain17/17; thread-ID calls2.

One64 KiB RW,non-executable arena is explicitly allocated and charged to the
same48 MiB guest-image cap. Total image backing rises from31,621,920 to31,687,456
bytes. Requested/live-rounded bytes are44/64; allocation calls2,failed0,free0,
live allocations2. The64-byte combined allocation span is validated word by
word: only the embedded mutex protocol at60000018 is2; other words are0.
No neighboring guest holes are made accessible.

The new native heap uses host-side metadata, aligned first-fit splitting and
coalescing of adjacent free spans within an arena. Free requires exact ownership
and rejects interior,foreign,double or truncated wide pointers. Zero-byte malloc
owns a16-byte minimum block. Alignment16..65536,power of two. Size overflow,
host-backing failures,heap-cap exhaustion,image-cap exhaustion,guest collisions,
and arena/metadata capacity exhaustion return explicit errors/null output.
Freed blocks are reusable; physical arena bytes remain charged until teardown.

Initial backing64 KiB,growth page size4 KiB,heap cap8 MiB,at most8 arenas and
1024 descriptors. The arena/metadata limits can fail before the byte cap.
The image owns all backing. At bounded stop/failure,heap metadata detaches and
native mutexes clear; guest bytes remain inspectable until image destruction.
`ds_heap_destroy` can instead release exact owned anonymous arenas immediately,
using the new pointer/extent-checked `ds_image_remove_zero_region`. ELF and
mismatched/foreign backing are protected. No flat VM pointers or implicit commits.

Two original bodies were reviewed and pinned:0168ABD0 and0167AC88 (chunk011).
The latter is a preexisting ds_malloc host override, not recovered retail malloc.
The native bridge validates the guest context,original callsite,TOC and reviewed
request sequence. It never substitutes for the unsupported NULL-pool bootstrap
fallback. Body total192;61 original bctrl annotations in27 bodies. Chunks unchanged.
The generic backend supports free/reuse; the guest free wrapper is not selected.
OOM stops explicitly; original C++ new-handler/exception paths are not served.

Validation includes randomized allocation/free/payload preservation,alignment,
coalescing,reuse,zero-size,overflow,metadata/arena limits,physical-provider failure,
shared budget charging,ownership/ELF protection,and image teardown. Integration
tests cover real allocations and original caller initialization,fresh repeats,
replay refusal,corrupted heap mutex state,fault after native create,backing-provider
failure,and wrong allocation ABI. Earlier checkpoints and image mutations run
under ASan/UBSan/LSan. Candidate/source/test evidence is frozen in
device-backed-heap-v0200/ once required checks complete.

All12,321 sanitizer checks and nine compiled lifter branch/link cases pass.
The strict devkitA64/libnx build and ZIP CRC/content checks pass. Candidate:
device-backed-heap-v0200/candidate.nro,460,997 bytes,SHA256
993a59244d444bc63ded33234f1093b35f484c172bb1c04bb8cfe3d855b9b8cd.
The current NRO,SD folder and ZIP all match the sealed candidate. Source and
test-log hashes are in build-verification.json.

After USB reconnection,v0.20 was installed through DBI MTP on2026-10-25 18:59 UTC.
The complete460,997-byte readback matches the sealed candidate exactly. Receipt:
transfer-verification.json; installed-readback.nro preserves the retrieved copy.
The prior binary matched the verified v0.19 candidate. Its newer log is saved
as pre-install-loader.log and matches the verified v0.19 log except advancing
clock values,whose conversion and monotonicity were checked. The initial
disconnected-device attempt made no remote changes.

v0.20 passed twice on the Switch,including fresh reload:all2,931 independent
checks pass without mismatches. Both658-entry/12,247-access traces,stop ABI,
two real allocations,13 CRT mutexes,native cleanup and budget accounting match.
Each run commits65,536 heap bytes,bringing total image backing to31,687,456
within the50,331,648-byte cap. Metadata clears after each checkpoint; final
guest backing is0. The hardware clock advances by9,225,046 microseconds across
reload. The post-run installed NRO still matches the sealed candidate exactly.
Evidence:loader.log,device-result.json,and postrun-verification.json.
No further v0.20 device action is pending; MTP is released after retrieval.

Next code dependency:locale consumer016896C0 and its unreviewed callees.
