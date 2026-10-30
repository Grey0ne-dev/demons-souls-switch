# Current v0.27 handoff — 2026-10-28

v0.27 completes original wide initialization AND startup constructor thirteen.
All eight narrow/wide streams initialize and register. Original conversion and
TLS/error helpers run; no host character result or new native service added.
New API ds_entry_run_wide_streams / stop38 DS_ENTRY_WIDE_STREAMS_READY stops
before constructor14: OPD0194CF48,code00745248,TOC019B5040. Boundary:
r3=0,r4=0,r5=1,r6=0FEFFB24,LR016B84F4,SP0FEFFE00,lastNID1BC200F4.
Thirteen completed constructors,1,265 entries/16,590 accesses. Target14 is
neither selected, entered nor counted. All15 prior APIs retain exact stops.

v0.27 installed via DBI MTP at2026-10-28 19:03 UTC,exact530,629-byte readback.
SHA2562eed820a0564ed624dfa83021863fa1c3e8485f5413a8d084f0a633cee35e196.
Previous installed binary AND log exactly match verified v0.26 and are backed
up as pre-install-loader.nro/.log,with verification receipt. Current seals,
source snapshots and evidence hashes match. MTP released. User was instructed
Loader/checkpoint/A/checkpoint/+/DBI. Hardware run is PENDING: no v0.27
hardware pass or device-result exists yet. Latest verified hardware is v0.26.
Next: retrieve v0.27 loader.log via MTP, run validate_device.py, read back NRO
again, verify sealed source/snapshot/evidence hashes, record postrun receipt,
release MTP, update status. Do not treat transfer as hardware execution.

All9,926 ASan/UBSan/LSan checks pass:589 full regression,549 clean integration,
8,386 original-conversion cases,67 six-check faults. Original-source/ELF model:
567 state/metadata words +20 instruction checks. Nine compiled lifter tests
and strict Switch cross-build/package checks pass. Fault fixture was replaced
by clean generation AND executable and retested; generated source matches
production. All jobs complete. Sealed sources/snapshots/scripts/evidence in
device-wide-streams-v0270/. Do not modify sealed files.

New full body pins:0168BC68,0168E548,0166CA50,0166FE88,00B17510,00010C50,
0166C0E8,01664E60.223 bodies;65 original bctrl sites in29 bodies. Earlier215
body hashes and63 sites unchanged. Third verified cross-TOC thunk00010C50:
F8410028/3C420002/3842EB6C/4965B48C ->0166C0E8; neighbor source retained
unreachable. New wide virtual return0168E690 validates OPD01967FD0/code00B17510,
facet60000240,r4=32,SP0FEFFB40,allocations22..25,no held locks and actual
main TOC. Wrapper and caller restore their original TOCs. No generic widening.
Generated-host-code parentheses warning suppressed specifically; original
instruction bodies remain unchanged. Normal native code uses strict warnings.

25 exact allocation contexts:420 requested/672 rounded/65,536 backed bytes,
same total guest backing31,687,456. Core locale refs16; narrow/wide facets each1.
Wide facet60000240 stores TLS0E000310/0E0003CC. Wide cleanup node60000260
links to old600000F0; callback already registered,so atexit free slots stay78.
Registry slots1..8 reference all streams; original wide fill0020. Fifteen earlier
CRT records plus three wide mutexes =18;58 thread-ID calls,66/66 locks,held0.
Shared CRT capacity16->32 and trace1024->2048; bounds/ownership/cleanup remain.
The five original64-bit counters become1; all flags remain0.

Next implementation: constructor14 wrapper00745248 (7 lines,setsr3=1,r4=FFFF,
tail00744E88). Wrapper inspected only;00744E88 remains unreviewed/unselected.
No game graphics,audio,scheduling,stream I/O,input or gameplay. Current docs:
SWITCH_WIDE_STREAMS_STATUS.md and WIDE_STREAMS_AUDIT.md.

No agents spawned. No AGENTS.md;root not Git. Follow current developer mode.
GIO needs escalation for desktop D-Bus. Plain gio copy replaces targets;
--overwrite unsupported. User's existing MTP authorization persists.
URI:mtp://-_DBI_XJJ10003310525/1: SD Card/switch/demons-souls-loader/

## Previous verified checkpoint (historical)

# Current v0.26 handoff — 2026-10-28

v0.26 is installed through DBI MTP with exact 514,245-byte readback verified.
SHA256 9ae63541f909d998bb0a8cc294c3e41a7bbc8a84eb9a3ed29efb9341df6627ab.
Transfer completed 2026-10-28 18:34 UTC. Prior installed loader AND log matched
verified v0.25 exactly and are preserved as pre-install-loader.nro/.log.
Receipt: pre-install-verification.json. MTP is released.

v0.26 PASSED twice on the actual Switch, including fresh reload. All 4,308
independent device checks pass with zero mismatches: full 966-entry /
14,378-access traces, exact ABI, thirteen allocations, all four narrow-stream
registrations, locale/facet/cleanup state, 42/42 locks and complete metadata /
guest-backing cleanup. Hardware clock advances 7,478,061us. Post-run binary
matches the sealed candidate; sealed sources, snapshots and evidence hashes
remain intact. Evidence: device-narrow-streams-v0260/loader.log,
device-result.json and postrun-verification.json. No device action pending.

New API ds_entry_run_narrow_streams / DS_ENTRY_NARROW_STREAMS_READY (stop37)
completes original01686CE8 and stops before entering/counting wide0168BC68.
ABI:r3=01C5BE41,r4=0,r5=0,r6=01CE6658,LR015165E8,SP0FEFFD90,
TOC019D3BAC,lastNID1BC200F4. Twelve constructors complete; thirteen incomplete.
All215 original body pins and63 indirect sites in28 bodies remain unchanged.
Only a mode-specific wide-entry checkpoint is added; no original body selected
or regenerated. No generic TOC dispatch acceptance widened.

Five added allocations:4/40/4/4/4 at60000100/110/140/150/160. Each is guarded
by sequence/size/caller/SP/TOC before counting/allocating; request14 is rejected.
Thirteen total allocations:232 requested/368 rounded/65536 backed bytes, same
31,687,456 total guest backing. Locale refs9,facet refs1,15 CRT mutexes,
31 thread-ID imports,42/42 locks,held0. Registry slots1..4 point to
01CE65D0/01CE6608/01CE66B4/01CE66EC; original space fills and buffer/tie fields
are validated. First and final virtual sites use exact descriptors/targets/
arguments/stacks/counts and original TOC transitions. No guest destructor runs.

All1,109 ASan/UBSan/LSan checks pass:589 full regression,322 clean integration
(including14 prior metadata/runtime APIs),33 six-check faults. Nine compiled
lifter tests and original-source/ELF model checks (258 words +9 instructions)
pass. Strict Switch build, ZIP CRC and manifest checks pass. Fault fixture
replaced by clean generation and executable; production generation matches.
All jobs completed. First regression was interrupted when the runner was edited
while executing; completed stable rerun is the accepted evidence. Failed run
and explanation retained. Do not edit runner files while executing them.
Sealed sources additionally preserved under source-snapshot/. Do not alter
sealed candidate/evidence/snapshots. Build receipt remains historically
pre-device; separate device/postrun receipts establish hardware success.

Next code: separately audit wide0168BC68/0168E548 and their allocation / wide
conversion dependencies. Inspected but unselected: TLS helpers0166CA50/
0166FE88, virtual00B17510, cross-TOC thunk00010C50, conversion0166C0E8.
Wide streams require a new audit; native CRT registry currently limited to16
slots and function trace to1024 entries, so later expansion must be reviewed.
Constructor thirteen is not complete. No I/O, graphics, audio, scheduling,
input or gameplay. Docs: NARROW_STREAMS_AUDIT.md / SWITCH_NARROW_STREAMS_STATUS.md.

No agents spawned; follow current developer mode. No AGENTS.md found; root not
Git. GIO needs escalation for desktop D-Bus. Plain gio copy replaces targets;
--overwrite unsupported. User authorized continuation and ran v0.26 twice.
URI:mtp://-_DBI_XJJ10003310525/1: SD Card/switch/demons-souls-loader/

## Previous verified checkpoint (historical)

# Current v0.25 handoff — 2026-10-28

v0.25 is installed through DBI MTP with exact 506,053-byte readback verified.
SHA256 4c5506e323deb47a636571adacf0d3b9f95ecdc0cc4c22840a6d78f69d00cd6c.
Transfer completed 2026-10-28 18:12 UTC. The prior installed binary AND log
exactly matched verified v0.24 and are preserved as pre-install-loader.nro/.log,
with pre-install-verification.json recording their hashes. MTP is released.

v0.25 PASSED twice on the actual Switch, including fresh reload. All 3,656
independent device checks pass with no mismatches: both full 810-entry /
13,312-access traces, exact ABI, unchanged eight allocations, first stream
registration/fill, locale/facet/cleanup state, balanced 26/26 locks, metadata
cleanup and final zero guest backing. Hardware clock advanced 6,220,263us.
Post-run installed binary matches the sealed candidate; sealed source/evidence
hashes remain intact. Evidence: loader.log, device-result.json and
postrun-verification.json in device-first-stream-v0250/.
No further v0.25 device action is pending.

New API DS_ENTRY_FIRST_STREAM_READY / ds_entry_run_first_stream validates
stop36 before entering/counting allocator0168ABD0 for the ninth four-byte
runtime request. ABI: r3=4,r4=01CE6558,r5=1,r6=7F7F7F7F7F7F7F7F,
SP0FEFFA00,LR01685FEC,TOC019D3BAC. Twelve constructors complete; thirteen
and the second narrow stream remain incomplete. Last NID1BC200F4.

Original virtual009E96A0 returns32. Original registration01684D70 publishes
stream01CE65D0 in slot one at01CE6498, increments byte01CE6489 to1, stores
index1 at01CE65D4. Full0x60-byte registry span checked, including padding.
Fill byte01CE6600=20; second stream prologue01CE6604/08=018C1B1C/018C1B30.
The first narrow stream constructor returns. Eight allocations unchanged:
40/4/52/40/4/4/24/8; requested176/rounded256/backed65536; total image backing
31,687,456. Locale refs5; facet refs1. Fourteen CRT mutexes, fifteen thread-ID
imports,26/26 locks,held0. All native/heap metadata and final backing cleanup pass.

215 full pinned bodies;63 original bctrl sites in28 bodies unchanged. Only
009E96A0 and01684D70 added. All prior body hashes, explicit checkpoint guards
and cross-TOC thunks unchanged. Only later-call boundary01684D70 removed.
No generic TOC acceptance widened. Original descriptor loads main TOC before
virtual call; original code restores runtime TOC afterwards.

All822 ASan/UBSan/LSan checks pass:589 full regressions,158 integration
(including all thirteen prior metadata/runtime APIs),15 five-check faults.
Nine compiled lifter checks and14 original ELF/body audit assertions pass.
Strict Switch build and package CRC/manifest checks pass. Clean annotated
exploration source AND executable were restored after fault tests and passed;
generated source matches production exactly. All jobs finished; none pending.
Sealed candidate/scripts/evidence in device-first-stream-v0250/; do not alter
sealed files. Device results are separate from historical pre-device build receipt.

Next code milestone: audit and accept five remaining narrow-stream allocation
contexts in FIRST_STREAM_AUDIT.md, completing01686CE8. This does NOT complete
constructor thirteen: its caller next enters wide initializer0168BC68 and
0168E548, which need separate dependency review. Registered cleanup016887C8
remains unselected/uninvoked. No stream I/O, graphics, audio, scheduling, input
or gameplay is implemented. See SWITCH_FIRST_STREAM_STATUS.md.

No agents spawned. Follow current developer agent mode. No AGENTS.md found;
workspace root is not Git. GIO needs escalation for desktop D-Bus. Plain gio
copy replaces existing targets; --overwrite unsupported. User explicitly
reported Switch plugged in/MTP active and ran the v0.25 test twice.
URI:mtp://-_DBI_XJJ10003310525/1: SD Card/switch/demons-souls-loader/

## Previous verified checkpoint (historical)

# Current v0.24 handoff — 2026-10-26

v0.24 is installed through DBI MTP with exact506,053-byte readback verified.
SHA2561b9aaee6378998e1ec1ab66e3643d95ec8e62ad407c264b11e96dfc7d47e61fc.
Transfer completed2026-10-26 09:40 UTC. MTP is released. Sealed source/evidence
hashes remain intact. Prior installed binary AND log exactly match verified
v0.23 and are preserved as pre-install-loader.nro/.log; hash receipt is saved.

v0.24 PASSED twice on the actual Switch, including fresh reload. All3,591
independent checks pass with no mismatches: both full799-entry/13,238-access
traces, exact ABI, eight allocations, first facet/cache/cleanup-node state,
25/25 locks, metadata cleanup and final zero guest backing. Hardware clock
advanced7,017,434us. Post-run installed binary matches the sealed candidate;
sealed source and evidence hashes remain intact. Independent read-only review
found no material contradiction between device validator and sealed checkpoint;
separate NRO/seal verification is recorded in postrun-verification.json.
Evidence:loader.log,device-result.json and postrun-verification.json in
device-first-facet-v0240/. MTP is released. No further v0.24 device action pending.

New API DS_ENTRY_FIRST_FACET_READY / ds_entry_run_first_facet validates stop35
before virtual009E96A0 at OPD01970348 and returnPC01688280. Target is NOT
selected or entered.799 entries/13,238 accesses,r3=600000D0,r4=32,r5=1,
r6=7F7F7F7F7F7F7F7F,SP0FEFFA90,LR01688280,TOC019B5040. Twelve constructors
complete;13 and stream constructor incomplete. Last NID1BC200F4. Thread-ID14,
locks25/25,held0,all native/heap metadata and final backing cleanup pass.

Original code constructs24-byte facetD0 and8-byte cleanup nodeF0, copies84
locale bytes to stackFEFFB08, creates/restores C strings, publishes cache1B50430,
registers atexit callback19AC6D0 at1CE58E4 (free slots78), links node at1CE6720.
Eight exact requests40/4/52/40/4/4/24/8,requested176/rounded256/backed65536;
image backing31,687,456 unchanged. Core locale refs5,facet refs1. CRT mutex14.
All256 heap bytes,21 copied locale words,defined destroyed string fields,both
aggregates and all prior metadata are checked. No registered destructor invoked.

213 full pinned bodies;63 original bctrl sites in28 bodies unchanged. Added
six reviewed full bodies0168A6B0/0167AED0/01668208/01668E50/016888D8/016884C0.
Original chunks untouched. Virtual boundary uses existing typed-dispatch checks;
no generic TOC acceptance widened. Original descriptor loads main TOC normally.
All8 accepted allocations check exact size/caller/stack/TOC/sequence.

All17,145 ASan/UBSan/LSan checks pass:589 full regressions,127 integration
including all12 prior checkpoint APIs,16,384 original comparator oracle/ABI
checks,and nine5-check faults. Nine compiled lifter tests pass. Strict Switch
build and package CRC/content checks pass. All jobs completed; no pending
processes. Clean annotated first-facet exploration source AND executable restored
and passed127 checks after faults; repeated checks not double-counted.
Production build/entry separate and clean. Sealed artifact/scripts/evidence in
device-first-facet-v0240/. Do not change sealed candidates/evidence.

Comparator test harness initially used seven7F bytes,causing34 failures. It was
corrected to actual eight-byte ELF mask; no original/production comparator body
changed. Failed evidence plus correction record are preserved. All16,384 cases
pass with corrected harness. The actual facet cache is1B50430 viaTOC+4C44;
previousv0.23 docs incorrectly named unrelated word1CE6728 as default pointer.
New audit/status correct that classification; older sealed sources untouched.

Next virtual009E96A0 (5 lines,returnsr4;chunk005;SHA256
604214d12ee81dc15fea105e242774d9283e0d50b016758be4fcec991660ead5) was
fully inspected but NOT selected. Its OPD01970348 is atvtable01872C50+18hex;
neighbor019703A8 at+14hex is different range method009E9648 (unselected).
01684D70 stream-registration helper was reviewed previously but NOT selected.
Registered cleanup016887C8 and later stream/constructor allocation paths need
review. All graphics,audio,scheduling,input,stream I/O and gameplay remain absent.
See SWITCH_FIRST_FACET_STATUS.md and NEXT_CONSTRUCTOR_AUDIT.md.

Agent use follows current developer mode; v0.24 validation included a read-only
independent review. No AGENTS.md found;root not Git. GIO needs escalation for
desktop D-Bus. Plain gio copy replaces existing targets; --overwrite unsupported.
URI:mtp://-_DBI_XJJ10003310525/1: SD Card/switch/demons-souls-loader/

## Previous verified checkpoint (historical)

# Current v0.23 handoff — 2026-10-26

v0.23 is installed through DBI MTP with exact497,861-byte readback verified.
SHA2565d513cc6632739b449f3180506c04e878d0738a5eb8faa451fb13487f8449fc7.
Transfer completed2026-10-26 09:03 UTC. MTP is released. Sealed source/evidence
hashes in device-stream-locale-v0230/build-verification.json remain intact.
Prior installed binary AND log exactly match verified v0.22 and are preserved
as pre-install-loader.nro/.log; pre-install-verification.json records hashes.

v0.23 PASSED twice on the actual Switch, including fresh reload. All3,353
independent checks pass with no mismatches: both full747-entry/12,849-access
traces, exact ABI and last NID350D454E, six allocations, both real mutexes,
stream/base locale state, held-lock cleanup and final zero guest backing.
The hardware clock advanced3,533,977us. Post-run installed binary matches the
sealed candidate; all sealed source/evidence hashes remain intact. Evidence:
loader.log,device-result.json and postrun-verification.json in
analysis/device-stream-locale-v0230/. MTP is released. No further v0.23 device
action is pending. Older pending/hardware notes below are historical.

New API ds_entry_run_stream_locale validates stop34 before24-byte facet request
0168ABD0 after747 entries/12,849 checked accesses.12 constructors completed;
13 and stream constructor remain incomplete. ABI:r3=24,r4=1,r5=0,r6=01CE6564,
LR01688380,SP0FEFFA90,TOC019D3BAC. Actual last boundary NID350D454E:inner
wrapper recursion calls thread-ID but does not acquire/release a second native
mutex. Thread-ID10,locks23/22,held1; cleanup releases ownership and all registries.

Six actual allocations40/4/52/40/4/4 at60000000/30/40/80/B0/C0. Requested144,
rounded208,backed65536 bytes; total image backing31,687,456 unchanged. Locale
C/TLS/global pointers preserved;refs6; two new stream/base handles reuse TLS
object60000040. CRT mutex14 at60000090. Both aggregates, full208 heap bytes,
all earlier metadata, exact ABI and full trace are checked. Further allocation
is not served. Allocation guards check exact size/caller/stack/TOC/sequence;
old v0.22 and earlier APIs retain their exact boundaries/counts.

Four newly pinned bodies01688138,01685FC8,01685248,01688538 bring production
to207 bodies;63 indirect sites in28 bodies. Original chunks unchanged. The two
new bctrl markers in01688138 do not execute on the accepted path. No native
virtual target or HLE service was added. Evidence includes original ELF li/bl
instructions for all three new allocation callers and next unserved24-byte request.

All791 ASan/UBSan/LSan checks pass:589 full regressions,84 stream tests,78 prior
publication/metadata checks,eight5-check faults. Nine compiled lifter checks pass.
Strict devkitA64/libnx build and package CRC/content verification pass. All test
and build jobs completed; no pending processes. Clean annotated stream-locale
exploration source AND executable are restored after faults and pass84 checks;
those repeated checks are not counted twice. Production build/entry is separate
and clean. Sealed candidate/evidence/scripts are in device-stream-locale-v0230/;
do not alter them. Older sealed artifacts are preserved.

Next code frontier:24-byte facet constructor0168A6B0 and its registration,
destruction and virtual methods. Bodies0168A6B0/016884C0/016888D8/01684D70 were
extracted only to /tmp for dependency summaries; their full bodies have NOT
been reviewed or selected. Error/exception,string growth,guest free,memmove,
waiting,stream I/O,files,graphics,audio,scheduling and gameplay remain unsupported.
See SWITCH_STREAM_LOCALE_STATUS.md and NEXT_CONSTRUCTOR_AUDIT.md.

Do not spawn agents unless explicitly authorized. No AGENTS.md found; workspace
root is not a Git repository. GIO needs escalation for desktop D-Bus. Plain gio
copy replaces existing targets by default; --overwrite is unsupported. URI:
mtp://-_DBI_XJJ10003310525/1: SD Card/switch/demons-souls-loader/

## Previous verified checkpoint (historical)

# Active port continuation — 2026-10-26

## Current v0.22 handoff (supersedes older development/pending notes)

v0.22 is installed through DBI MTP with exact 481,477-byte readback verified.
SHA256 8cae8c3db80720a2be90183520f6a80f6576abe3afea9208343154bea00adcf8.
Transfer completed 2026-10-26 08:32 UTC. MTP is released. Source/evidence hashes
in device-locale-published-v0220/build-verification.json remain intact. Prior
installed binary AND log match verified v0.21 and are preserved as
pre-install-loader.nro/.log. The first copy command used an unsupported GIO
--overwrite flag and made no remote change; corrected plain `gio copy` replaced
the file, then exact readback succeeded. No rollback or partial transfer remains.

v0.22 PASSED twice on the actual Switch, including fresh reload. All 3,121
independent checks pass with no mismatches: both full697-entry/12,492-access
traces, exact ABI, C locale/TLS/global/handle publication, unchanged three
allocations and budget accounting, 19/19 locks with no held ownership, metadata
cleanup and final zero guest backing. Hardware clock advanced14,393,912us.
Post-run installed binary exactly matches the sealed candidate; sealed source
and evidence hashes remain intact. Evidence:loader.log,device-result.json and
postrun-verification.json in device-locale-published-v0220/. MTP is released.
No further v0.22 device action is pending. The v0.21 pass notes below are historical.

New API ds_entry_run_locale_published stops33 before the next40-byte request
at0168ABD0 after697 entries/12,492 accesses,12 constructors completed;13 pending.
ABI r3=40,r4=01AD58D0,r5=01CE6510,r6=01CE6500,LR016A051C,SP0FEFFB60,
TOC019D3BAC. Original string code constructs `*`, then assigns `C`. Object60000040
has refcount3,flags63,size1,capacity15. Original TLS setter publishes0E000430;
original code publishes handle60000030 and globals01CE672C/01CE6730. No state
seeding or string override. All128 heap bytes and120 aggregate bytes are checked.
Locks19/19,thread-ID5,no held native locks at stop. Actual requests40/4/52 remain
unchanged:96 requested/128 live-rounded/65,536 backed bytes in one RW/non-X arena.
Total backing31,687,456 within50,331,648 cap. All metadata/native registries clear;
image destruction releases backing. Constructor13 has not returned.

203 original bodies selected;61 indirect sites in27 bodies unchanged. Seven
added full hash-pinned bodies:01676088,00F17448,00F172D0,00179118,00011720,
01666594,016B7F40. Both cross-TOC thunk four-word ELF sequences and tail targets
are checked by generator and included in provenance. Neighboring unreachable
sequences remain unchanged; original source chunks were not modified/regenerated.
Older checkpoint boundaries are enforced before selected function entry counting.

All8,383 ASan/UBSan/LSan checks pass:589 full regressions,78 publication checks
(including ten older checkpoints),7,681 original-copy oracle checks,seven5-check
fault fixtures. Nine compiled lifter checks pass. Strict Switch build and ZIP
CRC/content checks pass. Sealed candidate/evidence/scripts are in
device-locale-published-v0220/. Do not alter them. The clean annotated exploration
source AND executable are restored after fault testing and pass78 checks again;
this repeat is not double-counted. Production build/entry is separate and clean.
All test/build jobs finished successfully; no pending processes remain.

Next frontier: fourth runtime allocation (another40-byte mutex) from the existing
016A0500 caller. Audit additional01686CE8 allocation callers and remaining locale
aggregate construction before accepting them. Current bridge permits only the
first three audited allocations. String growth, guest free/new-handler/exception,
memmove paths remain unsupported. Files, graphics, audio, scheduling, input and
full boot/gameplay remain unimplemented. See SWITCH_LOCALE_PUBLISHED_STATUS.md,
NEXT_CONSTRUCTOR_AUDIT.md and port READMEs.

Do not spawn agents unless explicitly authorized. Workspace root is not a Git
repository and no AGENTS.md was found. GIO needs escalation for desktop D-Bus;
plain `gio copy` replaces an existing target by default. MTP URI remains:
mtp://-_DBI_XJJ10003310525/1: SD Card/switch/demons-souls-loader/

## Historical handoffs (superseded by v0.22 above)

# Active port continuation — 2026-10-25

## Current v0.21 handoff (supersedes older development/pending notes)

v0.21 is installed through DBI MTP with exact469,189-byte readback verified.
SHA256950d3e593d11883f5b300ddceea9c82cc45a24388e480468485dbad405215d46.
v0.21 passed twice on the actual Switch, including fresh reload. All 2,993
independent checks passed: both full 669-entry/12,314-access traces, exact ABI,
locale scalar fields, three allocations, 18/17 locks with one held at stop,
native/heap metadata cleanup and final guest backing zero. The hardware clock
advanced 7,505,084 microseconds. Post-run binary matches the sealed candidate;
sealed source/evidence hashes also match. Evidence: loader.log, device-result.json
and postrun-verification.json in device-locale-object-v0210/. MTP is released.
No further v0.21 device action is pending. Prior installed binary/log matched sealed v0.20
exactly and are preserved as pre-install-loader.nro/.log with verification.

New API ds_entry_run_locale_object stops32 before01676088 after669 entries/
12,314 accesses,12 constructors completed.13 incomplete. Four newly selected
full hash-pinned bodies016896C0,016891F0,016B7F30,01689180(chunk011).
196 total;61 bctrl sites in27 bodies. Original chunks unchanged. Third audited
operator-new request52 at LR016892DC returns60000040; requests40/4 unchanged.
Locale scalar fields:vtable018C7DE0,refs1,string size0/capacity15. All128 heap
bytes validated. TLS0E000430 and public handle60000030 stay0; publication pending.
Stop ABI:r3=60000058,r4=017041B8,r5=0,r6=01AD8EE0,LR016891DC,
SP0FEFFA30,TOC019D3BAC. Thread-ID3,locks18/17,one held guard at stop,cleaned
on exit. Heap requested96/live128/backed65536;image backing31,687,456 unchanged.
Old v0.20 and v0.19 APIs preserve stops/counts; mode guards precede entry counting.

All943 ASan/UBSan/LSan checks and nine compiled lifter checks passed,including
589 full regressions,51 locale tests,four five-check fault cases,21 allocation,
38 runtime,224 older checkpoint checks. Faults:locale refcount,third allocation
size,post-lock null write,unmapped non-null TLS pointer. Strict Switch build and
ZIP CRC/content checks pass. Sources/logs/candidate frozen in
device-locale-object-v0210/build-verification.json; transfer-verification.json
records installation. Do not alter sealed candidates/evidence.

Next frontier:unselected cross-TOC string thunk01676088. Original four words at
01676088..94 are F8410028/3C42FFFE/38421494/4B8A13B4:save TOC,adjust by
-0x1EB6C to019B5040,tail branch00F17448. Pinned source includes unreachable
neighbor thunk sequences.00F17448/00F172D0 were read; their capacity/copy helpers
remain unreviewed/unselected. Initial string is `*` at017041B8,later `C` at0174FDE0.
No TLS seeding or fabricated string completion. See SWITCH_LOCALE_OBJECT_STATUS.md,
NEXT_CONSTRUCTOR_AUDIT.md,and device-locale-object-v0210/locale-audit.json.

Latest explore binary is clean annotated test_entry.cpp from --explore-runtime-heap,
and earlier checkpoints all passed on it. Production build/entry is separate and
clean. Both long test jobs finished successfully. No processes remain pending.

Continue implementation when requested. Do not spawn agents unless explicitly
authorized. Workspace root is not a Git repository; no AGENTS.md was found.

## Hardware and current artifact

Latest hardware result:v0.20 passed twice including fresh reload. All2,931
independent checks pass:no mismatches,658 entries/12,247 accesses,real40/4-byte
allocations at60000000/60000030,13 CRT mutexes,17/17 locks,thread-ID2,and exact
stop ABI at016896C0. Heap backing65,536 bytes;total image backing31,687,456
within50,331,648-byte cap. Metadata/native cleanup pass; final guest backing0.
Clock advances9,225,046us. Post-run installed NRO matches sealed candidate.
Evidence:device-backed-heap-v0200/loader.log,device-result.json,and
postrun-verification.json. No further v0.20 device input is pending. Release
MTP after retrieval. This supersedes all older pending-hardware notes below.

Current development:v0.20 serves actual malloc requests40 and4 at60000000 and
60000030. One64 KiB RW/non-X arena is explicitly backed and budget-charged;
image backing31,687,456 bytes,logical live64/requested44. Original code publishes
60000000 at01CE64EC and creates CRT mutex13 at60000010,protocol2 at60000018.
Stop30 before locale consumer016896C0:658 entries/12,247 accesses,
r3=60000030,r4=0176F780,r5=0,r6=01AD8EE0,LR01686E14,SP0FEFFBF0,TOC019D3BAC.
Twelve constructors have returned; thirteen incomplete. Locks17/17,thread-ID2.
New heap API(entry/ds_heap.c/.h):alignment16..65536,exact-owner free,reuse,
coalescing,overflow/OOM checks;cap8 MiB,8 arenas,1024 metadata blocks. All backing
is image-owned; metadata detaches at stop; image_destroy releases it. Explicit
heap_destroy releases exact pointer/extent-owned zero regions via new image API.
Pinned bodies192;61 sites in27 bodies. Only added0168ABD0 and0167AC88 original
malloc route; preexisting bootstrap fallback remains unsupported. Earlier APIs
retain their old stops, including no-allocation v0.19.
Evidence staged in device-backed-heap-v0200/. Tests --explore-allocated and
state-fault/native-fault/provider-fault/argument-fault; tests/run_heap_tests.sh.
All12,321 sanitizer checks plus nine compiled lifter cases passed. Strict Switch
build and ZIP content/CRC checks pass. The frozen v0.20 candidate is460,997 bytes,
SHA256993a59244d444bc63ded33234f1093b35f484c172bb1c04bb8cfe3d855b9b8cd.
Current NRO,ZIP and SD folder match it. Evidence/source hashes:
device-backed-heap-v0200/build-verification.json.
v0.20 is now installed via DBI MTP(2026-10-25 18:59 UTC),with exact460,997-byte
readback verified. Receipt:device-backed-heap-v0200/transfer-verification.json;
retrieved NRO:installed-readback.nro. Prior installed binary matched v0.19; the
newer v0.19 log is saved as pre-install-loader.log and matches its verified log
except correctly converted/advancing clock values. The disconnected attempt
made no remote changes. v0.20 execution is pending:Loader/PASS/A/PASS/+/DBI,
then retrieve loader.log and run device-backed-heap-v0200/validate_device.py.
Release MTP after transfer. Latest observed device pass remains v0.19; transfer
does not establish v0.20 hardware success. Older pending notes are superseded.
Next code frontier:full016896C0 body and016891F0/016B7F30 dependencies remain
unreviewed/unselected. Guest free wrapper and arbitrary new-handler paths are
not yet selected. See SWITCH_BACKED_HEAP_STATUS.md and NEXT_CONSTRUCTOR_AUDIT.md.

Latest hardware result:v0.19 passed twice including fresh reload. All2,873
independent checks pass:652 entries,12,206 accesses,twelve completed constructors
plus constructor13 locale prefix,two thread-ID calls,17/17 locks,exact locale
state/stop ABI,advancing clock and final guest backing0. Evidence:
device-runtime-heap-v0190/loader.log and device-result.json. Post-run installed
NRO matched candidate.nro. No further v0.19 hardware input is pending.
Validator-only correction:copied v0.18 count1 was stale; frozen trace indices44
and646 both call016BB2E4, requiring exactly2 thread-ID calls. Original validator
and failed result preserved; validator-correction.json records hashes/reason.
Sealed build manifest remains unchanged; use validate_device_pre_correction.py
when auditing its original validator hash. Candidate/trace were not modified.
This supersedes all older pending-device notes below.

Current development: v0.19 starts constructor thirteen's locale initialization
and validates a stop before operator-new0168ABD0,r3=40,LR016A051C,SP0FEFFB60,
TOC019D3BAC. Twelve constructors completed; thirteen has not returned. Exact
path652 entries/12,206 accesses; native locks17/17;locale counter01AD6020=1,
vtable01CE64E8=018C1BF0, allocation output01CE64EC=0. Shared state validation
retains ten names/forty keys and all previous objects/accounting. No new backing.
Approved bodies190, annotations60 sites in26 bodies. Four added bodies:
015166F0,01516590(chunk010),01686CE8,016A0500(chunk011); original chunks untouched.
New API ds_entry_run_runtime_heap and explicit mode; ordinary exploration still
rejects allocation. Tests:--explore-runtime-heap/state-fault/lock-fault/argument-fault.
All866 sanitizer checks and nine compiled lifter cases pass; strict Switch build
and ZIP content/CRC checks pass. Evidence/source hashes are frozen in
device-runtime-heap-v0190/. Candidate452,805 bytes,SHA256
3843cae625a4607a27cfe6deb7fe146132357e53fcbe8e49902a97e1b87c1ae9.
v0.19 is now installed through DBI MTP(2026-10-25 18:17 UTC); full readback
matches candidate.nro. Receipt:transfer-verification.json. Preserved prior log
matches the fully passing v0.18 loader.log. Current NRO,ZIP and sdcard are v0.19.
v0.19 hardware execution is pending:Loader/PASS/A/PASS/+/DBI, then retrieve
loader.log and run device-runtime-heap-v0190/validate_device.py. MTP is released
after transfer. See SWITCH_RUNTIME_HEAP_STATUS.md. Older pending notes below
are historical and superseded by this paragraph and the latest hardware result.

Next heap route:0168ABD0 ->0167AC88(preexisting ds_malloc host override), neither
selected. Future guest allocations need explicit backing/budget/alignment/free;
do not reuse the unsupported bootstrap allocator or return host pointers.

Latest hardware result: v0.18 passed twice including reload; all 2,817 checks
pass, with both full641-entry/12,134-access traces, exact ABI/resource totals,
advancing clock, native cleanup and final guest backing0. Device evidence:
device-twelve-constructors-v0180/loader.log and device-result.json. The initial
missing-exit log/result are preserved as incomplete-exit-* in that directory.
No further v0.18 device input is pending. This supersedes pending notes below.

Current development: v0.18 completes twelve constructors on the host, validates
ten names and forty sorted keys, and stops before OPD019A3EA0/code015166F0 with
runtime TOC019D3BAC. Exact path:641 entries/12,134 accesses;14 existing-pool
requests,five native creates,16/16 locks,1/1 writer; no new guest backing.
Approved bodies186; original chunks untouched; indirect sites58 in25 bodies.
Generator annotation and frontier selection now have independent options.
See SWITCH_TWELVE_CONSTRUCTORS_STATUS.md and NEXT_CONSTRUCTOR_AUDIT.md.
All833 sanitizer checks and nine compiled lifter cases pass; strict Switch build
and ZIP content/CRC validation pass. Preserved candidate and source/log hashes:
device-twelve-constructors-v0180/. NRO444,613 bytes, SHA256
44c730853e1f9f52e743f968d5dc7efc739d51f7f300dfc6d2bacdf81a88d210.
v0.18 is now installed through DBI MTP (2026-10-25 17:55 UTC), with exact
readback verified; transfer-verification.json and installed-readback.nro preserve
the evidence. The prior v0.17 log matches pre-install-loader.log. v0.18 hardware
execution is pending: Loader/PASS/A/PASS/+/DBI, then retrieve loader.log and run
device-twelve-constructors-v0180/validate_device.py. Release MTP after transfer.
Current NRO, ZIP and sdcard all match the sealed v0.18 candidate.

Next code frontier is constructor thirteen,OPD019A3EA0/code015166F0/TOC019D3BAC.
Only its wrapper was inspected; initialization body01516590 and dependencies
remain unselected. Keep non-main-TOC acceptance restricted to the exact stop.
All exploration shares build-host-explore-tests; regenerate before fixture
changes and never run its builds concurrently. Its current binary is clean
annotated production; --include-frontier has no unapproved bodies selected.

Latest hardware result: v0.17 passed twice including fresh reload. All 2,665
independent checks passed; 603 entries, 11,044 checked accesses, eleven completed
constructors, exact stop ABI, native cleanup, advancing hardware clock and zero
guest backing on exit. Evidence: device-eleven-constructors-v0170/loader.log and
device-result.json. Post-execution installed NRO readback matches candidate.nro.
No further v0.17 hardware input is pending. This supersedes older pending notes.

Historical installation notes (superseded by the hardware result above):
Update 2026-10-25 17:42 UTC: the preserved v0.17 candidate from
analysis/device-eleven-constructors-v0170/candidate.nro is now installed on the
Switch through DBI MTP, with exact readback verified (440,517 bytes, SHA256
a1c48cb43f96e8df39a7aae589ab2435db17cbeedd84922ce7d252b7c4b349d8).
See that directory's transfer-verification.json and installed-readback.nro.
The retrieved device log still belongs to v0.16 and matches pre-install-loader.log.
v0.17 device execution is pending: launch Loader, A for a fresh reload, + to exit,
then reopen DBI MTP so the new log can be retrieved and validated.
The v0.17 API completes eleven constructors (603 entries, 11,044 checked accesses)
and stops before constructor twelve at 003F46B0. Current source also contains
experimental constructor-twelve selections; use the preserved v0.17 binary for
this hardware test. Earlier artifact references below describe the v0.16 stage.

Latest observed Switch PASS is v0.16, twice including fresh reload. Evidence:
analysis/device-ten-constructors-v0160/loader.log, device-result.json and
validate_device.py. All 2,653 independent checks pass: ten constructors,
600 entries, 11,007 accesses, one actual ADF198 larger-sort partition entry per
attempt, 16/16 locks, five native recursive mutex creations, nine names and
36 keys. Both runs match the full expected trace and ABI. Native cleanup is
zero after each run; final exit reports zero guest backing. The hardware clock
advances across reload. Earlier v0.15 and other evidence remains preserved.

Current preserved v0.16 candidate:
analysis/device-ten-constructors-v0160/candidate.nro
440517 bytes, SHA256
fe8f07b84343ac43738afd282432b4067063d7a962a1a5b9e95c495a6e20bec7.
Installed over DBI with exact byte-for-byte readback, then MTP unmounted.
Current NRO, ZIP and sdcard are also v0.16. Device execution passed twice.
The prior v0.15 log is saved as pre-install-loader.log. Preserve sealed candidates.

USB installation authorization persists. Gio requires tool escalation for
session D-Bus. URI:
mtp://-_DBI_XJJ10003310525/1: SD Card/switch/demons-souls-loader/
User completed Loader/A/+/DBI. The log is saved and validated, and MTP was
released again. No further v0.16 hardware input is pending. Do not infer device
success from transfer alone in future versions.

## v0.16 implementation

175 original bodies are hash-pinned; indirect annotations remain 58 sites in
25 functions. Original source chunks were not modified or regenerated.
Ten reviewed bodies were added: FileCap wrapper 0066A4A8 and initializer
0066A2D8 (chunk003), parent getter005720A8 (chunk002), six scalar getters and
larger-sort partition helper00ADF198 (chunk005, 502 lines, no dependencies).
The original scalar sort bodies use checked accesses, signed comparisons and
pair swaps. No host sorting replacement is used in the Switch build.

New API ds_entry_run_ten_constructors returns stop26 before constructor eleven:
OPD01943CD0/code005FD720, LR016B84F4/SP0FEFFE00. Actual path:600 function entries,
11,007 checked accesses, nine type names and36 sorted opaque key/object pairs.
FileCap is a seven-byte type name; its root is1B49B70, parent1B47D88, vtable1864700.
No file I/O runs. Names are LuaMan6, FileCap7, shadow11, collector13, two prior17,
dialog18, scene23, shader33. FileCap keys1B49B94..97 sort between Lua and original
first type keys. All previous node/method/sentinel/counter data remains checked.

Resource totals unchanged:14 existing-pool requests, five native recursive mutex
creates,16/16 locks,1/1 writer,one atomic success. Pool free0x5CB48. No new arena.
All native registries clear at normal stop and tested failures. In-place replay
is refused. Heavy native mutex capacity8/current peak5; only create/contains/
live/reset exist, not kernel lock/wait services.

Metadata checkpoint constants now use explicit profiles for ctor counts3,4,6,9,
10: exact trace/access counts, stop ABI, names/keys, allocations and native totals.
Older APIs still pass. Trace mismatch is included in final validation so a
fixture adding an access can still identify its first corrupt memory word.

## Validation and commands

All2,005 checks pass this turn:589 full regressions,20 old three-ctor,23 old
four-ctor,32 old six-ctor,33 old nine-ctor,eight dispatch equivalence,22 ten-ctor,
five final-state faults,four partition-input faults,1,260 sort-matrix checks,
nine compiled lifter branch cases. First1,996 use ASan/UBSan/LSan. The image
backend is unchanged; older1,315 image mutations were not rerun this turn.
Strict devkitA64/libnx cross-build and ZIP CRC/content checks pass. Source hashes,
logs and preserved candidate are in device-ten-constructors-v0160/. The full
600-entry expected-device-trace.json adds39 tested host entries to the validated
v0.15 prefix. v0.16 passed twice on hardware with the full 600-entry match.

Sort matrix:tests/test_sort.cpp and runner --explore-sort-matrix. A host-only
callback seeds the real tenth-constructor outer-sort call after36 keys exist.
Original sort/partition bodies remain unchanged. It runs126 cases: counts0,1,2,
31,32,33,36,40,41,42,64,128,512; ascending,descending,equal,signed extremes,random,
organ-pipe patterns; plus depth-zero fallback for counts>32. It checks signed
order, complete key/payload multiset against std::sort, adjacent canaries,
partition/fallback use and native/guest cleanup. Duplicate ordering is not
assumed stable. Synthetic registry inputs never enter the production package.

bash switch-port/tests/run_entry_tests.sh [--explore-tenth|--explore-sort-matrix|
--explore-tenth-state-fault|--explore-tenth-sort-fault|--explore-ninth|
--explore-sixth|--explore-fourth|--explore-third|--explore-probe|--explore-ahead]
All exploration shares build-host-explore-tests; do not run two concurrently.
Default regression uses separate build-host-entry-tests. Last explore generated
source is a test-only PARTITION-INPUT-FAULT fixture; regenerate before probes.
Production build/entry is separate and clean. New faults corrupt final pair
object01AD8EBC after ten returns, or force partition begin0 and require a checked
read fault at0 in ADF198. Both clean all native resources. Bad FileCap getter
TOC is tested at019471C4 and rejects before constructor ten completes.

make -C switch-port -j1
python3 switch-port/tools/prepare_sd.py
python3 switch-port/tests/test_lifter_links.py
Use tool escalation for sanitizer executions here: LSan process checks fail in
the sandbox. Do not disable leak detection as a workaround.

## Next frontier

Constructor eleven:OPD01943CD0/code005FD720. Its body and dependencies have not
been reviewed or selected. The larger-sort gap is now handled and tested through
the actual512-pair allocation capacity on host; audit new dependencies and growth
paths before selecting them. Constructor trace capacity1024 (600 currently) and
checked-access watchdog4,000,000 remain bounded diagnostic limits.
Future heap pages need explicit backing and budget charging. Full game boot,
files, graphics, audio, game input, SPU runtime, scheduling and gameplay remain
unimplemented. No performance result is established. Prior under5% estimate was
explicitly a rough judgment toward playability, not a measured percentage.

Docs:SWITCH_TEN_CONSTRUCTORS_STATUS.md,NEXT_CONSTRUCTOR_AUDIT.md,
INDIRECT_DISPATCH_STATUS.md,port READMEs and earlier versioned evidence.
