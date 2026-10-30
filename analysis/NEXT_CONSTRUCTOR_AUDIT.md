# Current frontier: constructor fourteen — v0.27

v0.27 completes original wide-stream initialization and constructor thirteen.
Stop38 before constructor14 OPD0194CF48 / code00745248 / TOC019B5040:
1,265 entries/16,590 accesses,13 constructors complete. r3=0,r4=0,r5=1,
r6=0FEFFB24,LR016B84F4,SP0FEFFE00. All9,926 sanitizer checks, source model,
lifter and Switch build/package checks pass. Installed readback verified;
v0.27 hardware execution is pending. See SWITCH_WIDE_STREAMS_STATUS.md.

Next: audit full wrapper00745248 and tail target00744E88 plus its dependencies.
The seven-line wrapper was inspected:setsr3=1,r4=FFFF then tail00744E88.
The target remains unreviewed/unselected. Do not extend the constructor walker
limit or accept new allocations/services before the dependency audit. Existing
25 allocation contexts,32 CRT slots and2048 trace entries remain bounded.
Preserve all15 earlier APIs and sealed v0.27 source snapshots/evidence.
Rendering,audio,scheduling,input,stream I/O and gameplay remain absent.

# Previous frontier: wide-stream initialization — v0.26

v0.26 completed01686CE8 and passed twice on Switch:4,308 device checks,
zero mismatches. All four narrow streams initialize/register. Stop37 before
wide0168BC68:966 entries/14,378 accesses,13 allocations,232 requested/368
rounded bytes,42/42 locks. Twelve constructors complete; thirteen incomplete.
Exact boundary:r3=01C5BE41,r4=0,r5=0,r6=01CE6658,LR015165E8,
SP0FEFFD90,TOC019D3BAC. Full original-source/ELF state model matches.

Next: audit/select wide initializer0168BC68 and constructor0168E548, their
32-byte wide facet, allocation callers and virtual/conversion route. Inspected
but unselected helpers0166CA50/0166FE88 return TLS pointers; wide virtual
00B17510 reaches cross-TOC thunk00010C50 and conversion0166C0E8. Do not
substitute a wide character or bypass conversion. Audit original words, TLS
state, new allocation contexts and any needed registry/trace capacity changes.
Current native CRT capacity16 and trace1024 are bounded diagnostics, not an
assertion that wide startup fits. No wide body or new conversion service is
selected. Preserve v0.26 checkpoints and sealed evidence/snapshots.

Evidence:device-narrow-streams-v0260/. All1,109 sanitizer checks, nine compiled
lifter cases and source model checks pass. Current device and handoff:
SWITCH_NARROW_STREAMS_STATUS.md / CONTINUATION_STATE.md.

# Previous frontier: remaining narrow streams — v0.25

v0.25 passed twice on Switch: 3,656 independent checks, zero mismatches.
First facet virtual009E96A0 and stream registration01684D70 execute; first
narrow stream returns and registry slot one is published. Stop36 precedes
ninth allocation0168ABD0 after810 entries/13,312 accesses. r3=4,r4=01CE6558,
r5=1,r6=7F7F7F7F7F7F7F7F,LR01685FEC,SP0FEFFA00,TOC019D3BAC.
Twelve constructors complete; thirteen and second narrow stream incomplete.

Next: audit and accept the five remaining allocation contexts in
FIRST_STREAM_AUDIT.md:4/40/4/4/4 bytes at the exact reviewed callers/stacks.
Complete01686CE8 with original repeated facet/registration paths; then audit
wide initializer0168BC68/0168E548 before continuing constructor thirteen.
The audit's later allocation addresses/refcounts are predictions, not executed
coverage. Current bridge still serves exactly eight requests. No registered
destructor or guest free path added. Stream I/O and gameplay remain absent.

Production:215 pinned bodies;63 indirect sites in28 bodies. All822 sanitizer
checks and nine compiled lifter cases pass. Sealed v0.25 evidence is in
device-first-stream-v0250/; preserve it. Current hardware and installation:
SWITCH_FIRST_STREAM_STATUS.md and CONTINUATION_STATE.md.

# Previous frontier: first facet virtual method — v0.24

v0.24 constructs the first facet, restores original locale state and registers
its cleanup callback/node. Eight real requests40/4/52/40/4/4/24/8 use176
requested/256 rounded bytes in the same64 KiB arena. FacetD0 refs1,corelocale40
refs5. Cache1B50430=D0,cleanuphead1CE6720=F0,atexitfree78. Native25/25 locks
balanced. Stop35 after799 entries/13,238 accesses precedes virtual009E96A0
through OPD01970348 (vtable01872C50+18hex),bctrl0168827C,LR01688280,
SP0FEFFA90,TOC019B5040,r3=600000D0,r4=32,r5=1,r6=7F7F7F7F7F7F7F7F.
The target method was NOT invoked or counted. Constructor13 and stream
constructor have not returned;12 prior constructors completed.

Next: select/test the original009E96A0 method (five lines,returnsr4 unchanged;
chunk005,SHA256604214d12ee81dc15fea105e242774d9283e0d50b016758be4fcec991660ead5),
then review remaining stream registration/allocation routes before extending
constructor13.01684D70 registration helper was reviewed but remains unselected;
registeredcleanup016887C8 is not reviewed/selected. Do not substitute virtual
results or invoke registered guest callbacks through native pointers.

213 pinned full bodies;63 bctrl markers in28 bodies. Six reviewed originals
0168A6B0/0167AED0/01668208/01668E50/016888D8/016884C0 added. Original chunks
unchanged. No general TOC acceptance widened. All12 older checkpoint APIs retain
exact counts. Current source/evidence:device-first-facet-v0240/ and
SWITCH_FIRST_FACET_STATUS.md. Actual facet cache is1B50430; earlierv0.23 label
for unrelatedzero word1CE6728 was inaccurate and is corrected in current docs.

The notes below describe previous frontiers.

# Current frontier: first locale facet allocation — v0.23

v0.23 validates the original stream/base locale state before its first24-byte
facet request at0168ABD0. LR01688380,SP0FEFFA90,TOC019D3BAC,r3=24,r4=1,r5=0,
r6=01CE6564. It stops before entry after747 functions/12,849 accesses,with12
constructors completed;13 and the stream constructor remain incomplete.

Six real requests40/4/52/40/4/4 reuse one64 KiB arena;144 requested/208 rounded
live bytes. Locale C object60000040 has refs6; stream/base handles600000B0/C0
reuse it through original TLS code. Mutex14 at60000090;23/22 native locks,one
held guard cleaned at stop. All earlier metadata and exact old checkpoints pass.

Next: review full0168A6B0 facet constructor and dependencies before permitting
its24-byte allocation. Its call from01688138 is LR016883C0; later initialization
uses016888D8 and016884C0 and two currently unexecuted virtual sites. These bodies
were extracted for dependency summaries but have NOT been fully reviewed or
selected. No facet or virtual result has been fabricated. Exception/growth/free/
memmove paths stay unsupported. See SWITCH_STREAM_LOCALE_STATUS.md and
versioned device-stream-locale-v0230/ audit/provenance.

207 full original bodies are pinned;63 bctrl markers in28 bodies. Four added
bodies01688138/01685FC8/01685248/01688538 were reviewed in full and unchanged.
Only six exact size/caller/stack/TOC/sequence allocation requests are accepted.
The notes below describe previous frontiers.

# Current frontier: next locale aggregate allocation — v0.22

The original string/TLS path now runs: `*` construction, `C` assignment, real
TLS setter, both global pointers, public handle and reference count 3. All 19/19
locks release normally. Stop 33 precedes the fourth operator-new request (40
bytes at LR 016A051C / SP 0FEFFB60 / TOC 019D3BAC). It is not served. The
697-entry/12,492-access path still has twelve completed constructors; thirteen
is incomplete. The shared 64 KiB arena still backs only requests 40/4/52.

Next: audit the additional allocation callers inside the remaining 01686CE8
locale aggregate construction before widening the bridge. The earlier two
40/4-byte requests and one 52-byte object request remain the only accepted
calls. String growth, free/new-handler/exception and memmove paths remain strict
boundaries. Do not seed TLS or replace original string operations.

Seven complete hash-pinned bodies are added (203 total): 01676088, 00F17448,
00F172D0, 00179118, 00011720, 01666594, 016B7F40. Both thunk instruction ranges
and TOC transitions are verified against the ELF; merged neighboring sequences
are unreachable and unchanged. Indirect annotations remain 61 in 27 bodies.
See SWITCH_LOCALE_PUBLISHED_STATUS.md and device-locale-published-v0220/.
The notes below describe earlier frontiers.

# Current frontier: locale string constructor

v0.21 selects full original016896C0,016891F0,016B7F30 and01689180 bodies.
The real TLS getter returns null, lock zero is acquired, and a third52-byte
allocation at60000040 receives its original vtable/refcount/string capacity.
Stop32 is before01676088 after669 entries/12,314 accesses. The object and
all preceding state are validated; TLS/public-handle publication is still pending.
One lock is held at stop and released by cleanup; thread-ID calls3,locks18/17.
The same64 KiB backing serves96 requested/128 rounded bytes. Twelve constructors
completed; thirteen remains incomplete.196 bodies,61 sites in27 bodies.

Next audit:01676088 is a TOC-adjusting tail thunk into00F17448/00F172D0.
Its pinned source includes unreachable neighboring thunk sequences; check the
ELF instruction range and preserve provenance before selection. String capacity,
copy/move,free and exception helpers remain unselected. See
SWITCH_LOCALE_OBJECT_STATUS.md. Do not seed TLS or widen general malloc dispatch.

## Prior v0.20 frontier (superseded)

The v0.20 checkpoint serves original operator-new requests40 and4 via the pinned
0168ABD0 ->0167AC88 route. The native guest heap provides60000000 and60000030
from one charged64 KiB arena. Original016A0500 publishes the first pointer and
creates CRT mutex13. The path executes658 entries/12,247 accesses and stops at
016896C0,LR01686E14,SP0FEFFBF0,TOC019D3BAC,r3=60000030. Twelve constructor
returns; thirteen remains incomplete. Body total192;61 bctrl sites in27 bodies.

The start of016896C0 was inspected, but its complete body and dependencies have
not been approved. It calls016891F0 and016B7F30 before taking another runtime
lock and updating a reference count. Review its full implementation and these
callees before selection; do not fabricate locale state to bypass them.

The generic native heap supports aligned allocation,exact-owner free,reuse and
coalescing with independently bounded backing/metadata. The current guest bridge
accepts only the two audited malloc calls; guest free and new-handler/exception
paths remain unselected or unsupported. Do not broaden non-main-TOC dispatch or
the unsupported NULL-pool bootstrap path. Detail:SWITCH_BACKED_HEAP_STATUS.md.

Constructor twelve, OPD 0x01935910 / code 0x003F46B0, is now approved with its
initializer 0x003F44E0, six scalar accessors 0x003F3F10, 0x003F3F18, 0x003F3F30,
0x003F3F48, 0x003F3F60, 0x003F3F78, and shared parent getter 0x00CE3DF0.
It registers ImageProcessDrawEntitySeed, root 0x01B42878 / parent 0x01BB6060,
using ten names and forty sorted key/object pairs. The 641-entry / 12,134-access
path uses no new backing or native service. Unused 0x003F3F90 was omitted from
the final approved selection. Tests and ABI are in SWITCH_TWELVE_CONSTRUCTORS_STATUS.md.

Constructor eleven, OPD 0x01943CD0 / code 0x005FD720, is now selected with its
initializer 0x005FD628. It initializes three static eight-byte objects and one
sixteen-byte composite at 0x01B48C44..0x01B48C6B. The shared 0x00DBA100 body
was already selected. It sets the original vtables 0x01852728, 0x01852738 and
0x01852748 and zero payloads through checked big-endian accessors. It requires
no new service, native object or pool allocation. Both complete source bodies
were reviewed and hash-pinned; no original translation chunk changed.

The original larger sorter selected in v0.16 remains tested through 512 pairs.
Unknown later functions remain explicit boundaries.

## Historical constructor-ten audit before v0.16

# Constructor ten audit (not selected)

After the v0.15 nine-constructor checkpoint, the CRT selects OPD 0x01947270 /
code 0x0066A4A8. Its wrapper tailcalls 0x0066A2D8 in chunk 003.
The wrapper and complete initialization body were inspected; neither uses flat
VM pointers. The body initializes a metadata object, calls AE4D38, obtains parent
metadata through 0x005720A8, and links it through ADE478.

The type name in the verified ELF is `FileCap`. Registering its four keys would
take the key registry from 32 to 36 pairs. The existing selected sorter AE0BE8
uses in-body sorting through 32 pairs, then calls unselected larger-sort helpers.
That larger-sort path must be reviewed and tested before accepting constructor
ten. No body for constructor ten or these helpers has been added to selection.

Inspected source fingerprints:
- 0066A4A8, chunk 003: `474bc342c75ac92a9718aba4570b80a9a71fd611f70280609b55771255b96936`
- 0066A2D8, chunk 003: `ee41edb559c28535e949de3e9253b808ddae95fe0531c322b9f16b274ed38339`

Unselected direct references in AE0BE8: 00ADF198.
These dependencies are identified statically, not yet executed or reviewed.
