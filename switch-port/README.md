# Demon's Souls Switch port: bounded CRT startup

This directory loads the verified US v1.00 executable and runs a bounded slice
of its translated startup code on a portable segmented-memory backend. Version
0.27.0 completes thirteen static constructors, including all four narrow and
four wide C++ stream objects. The wide fill character is produced by original
conversion code and TLS state. Execution stops before constructor fourteen
(00745248): 1,265 entries / 16,590 checked accesses.

Twenty-five exact allocations use the existing 64 KiB backed guest arena;
all 66 locks release and all native/guest backing is cleaned on exit.
Graphics, menus and gameplay are not implemented. Current validation and
installation: analysis/SWITCH_WIDE_STREAMS_STATUS.md.

## Run

Extract `demons-souls-switch-startup.zip` onto the SD card root. It contains:

```
switch/demons-souls-loader/demons-souls-loader.nro
switch/demons-souls-loader/EBOOT.ELF
```

Open **Demon's Souls Loader** in the working Homebrew Menu. This bounded stage
uses ordinary allocated memory and needs no launch/config changes. The screen
and SD `/switch/demons-souls-loader/loader.log` report the result. **A** destroys
the current image and reloads; **+** releases it and exits.

The archive includes the existing locally supplied game input. It and the
extracted game-derived startup translations are local development artifacts.

## Implemented and verified

- ELF64 big-endian PowerPC loading, permission/range checks, entry/TLS validation,
  exact file hash and loaded segment/BSS hashes. The two ELF segments consume
  30,466,848 bytes of backing.
- Checked segmented accessors, bounded stack and TLS arenas, and explicit cleanup.
- 213 original function bodies are hash-pinned. Production adds verified
  indirect-callsite markers; 799 function entries run on the bounded default path. The upstream NULL-pool
  bootstrap patch in 0x00AC8078 is documented in provenance, never taken on
  the accepted path, and remains an explicit unsupported stop.
- Main-thread static TLS: real four-byte template, zeroed tail, biased thread
  pointer `0x0E007000`, and CELL_OK return.
- Import TOC save slots matching the pinned HLE dispatcher, so post-import
  TOC restores no longer load zero.
- Actual boot-path string and 64-bit argv/envp slots in a separate writable arena;
  the original CRT converts them to 32-bit pointers without changing RX code.
- Creation of thirteen 24-byte guest mutex records with the real protocol, backed
  by a native libnx mutex pool in the constructor mode (pthread mutexes on Linux).
  Uncontended main-thread lock/unlock is supported. Contention, timed waits, native
  recursion and guest scheduling are explicit unsupported paths.
- `sys_memory_get_user_memory_size` (352) writes two checked big-endian words
  and returns CELL_OK. Total/available memory come from the configured guest cap
  after charging all backed arenas, rounded down to 1 MiB. Default result: 48 MiB
  total, 17 MiB available. This is a conservative configured allowance, not console
  RAM or a guarantee that future host allocations succeed.
- Original stream/descriptor initializer instructions run, including all 253
  remaining descriptor slots and their auxiliary entries.
- `sys_time_get_system_time` (0x8461E528) uses the Switch hardware counter and
  Linux CLOCK_MONOTONIC in host tests, converted to microseconds with overflow
  checks. The original CRT stores this actual return value in its timestamp global.
- Audited indirect calls execute the first constructor and its virtual
  methods after validating their OPDs, code, TOC and executable backing. Their
  omitted bctrl LR values are restored from the actual verified instructions.
- The original atexit registration code stores destructor OPD 0x01967920 and
  decrements its free-slot count from 80 to 79 using a native mutex lock/unlock.
  The native thread-ID service writes the real main guest thread identity, 1.
- The original singleton/manager constructors initialize manager fields and
  its reader/writer object. sys_rwlock_create (120) validates the complete 32-byte
  attributes and output, initializes a libnx RwLock (pthread_rwlock on Linux),
  writes a registered guest handle and returns CELL_OK. This stage supports the
  observed private, priority-protocol creation and uncontended writer
  acquire/unlock only. Readers, IPC and waiter scheduling are not implemented.
- The original caller consumes the native handle, publishes manager pointer
  0x01B91128 at 0x01B9111C, and unlocks its startup mutex. Its configuration
  getter requests 0x60000 bytes at 0x01AD6B78, already backed by the ELF. The full
  interval is checked readable/writable; no backing is fabricated or allocated.
- Original pool initialization, free-list splitting, first allocation and range
  tuple insertion run against existing ELF backing. A checked big-endian native
  CAS supports this single guest thread; no multithread reservation semantics
  are claimed. Native mutex calls balance 4/4 and writer calls balance 1/1.
- Original name-registry initialization, a second 0x2000-byte pooled allocation
  and one 16-byte name-record insertion. The original checked `strlen` body
  returns length 17; the first 384-byte allocation remains intact. No new
  guest backing arena is added.
- A validated stop inside thirteenth constructor 0x015166F0, after its first
  thirteen runtime allocations, facet cleanup registration and all four narrow
  stream registrations, before wide initializer 0x0168BC68. Its runtime TOC 0x019D3BAC is accepted
  only for the reviewed constructor descriptor/code/count. The real locale
  counter, strings and first facet construction run. All guards release normally
  (42 acquisitions/42 guest releases). The original virtual descriptor loads
  main TOC 0x019B5040; original code restores runtime TOC after the method returns.
  Dispatch supports only
  the selected audited functions; page allocation and scheduling remain unsupported.
  Native mutex and reader/writer objects are cleaned on stop
  or failure. This stage requires a freshly reloaded image for each attempt;
  **A** already performs that reload.
- A native guest-heap allocator returns aligned guest offsets from explicitly
  backed RW arenas and charges the full backing against the image budget.
  Its first 64 KiB arena supplies eight requests; live rounded bytes are
  256. Original callers create two CRT mutexes, three locale handles, a facet and a cleanup node.
  Backend free rejects invalid ownership, coalesces adjacent free
  blocks within an arena, and reuses freed spans. Free does not release the
  arena's physical charge. Image teardown releases all backing. Independent
  heap/arena/metadata limits and allocation failures are tested.

Version 0.2's TLS checkpoint passed on the user's Switch twice, including reload
and zero-backed-byte cleanup, per the supplied device log. **Version 0.3 also passed on the user's Switch twice**, with seven function entries, 1,131 checked accesses, the expected
pre-syscall-352 boundary, and zero-backed-byte cleanup. Version 0.4 also passed twice on the user's Switch: the memory query completed,
ten mutex records were initialized, 31 function entries and 1,816 checked accesses
reached the time boundary without error, and cleanup released all guest backing. Host tests cover budget charging, clamping/rounding/exhaustion, actual
syscall output, all mutex records and descriptor slots, repeat runs, alternate
low-memory control flow, unknown services, and invalid output spans. Test fixtures
never enter production packaging. These establish no graphics, gameplay or
performance result.

## Build

```sh
python3 switch-port/tools/generate_reference.py
bash switch-port/tests/run_host_tests.sh
bash switch-port/tests/run_entry_tests.sh
bash switch-port/tests/run_entry_tests.sh --explore-probe
bash switch-port/tests/run_entry_tests.sh --explore-bad-callsite
bash switch-port/tests/run_entry_tests.sh --explore-third
bash switch-port/tests/run_entry_tests.sh --explore-fourth
bash switch-port/tests/run_entry_tests.sh --explore-fourth-fault
bash switch-port/tests/run_entry_tests.sh --explore-sixth
bash switch-port/tests/run_entry_tests.sh --explore-sixth-state-fault
bash switch-port/tests/run_entry_tests.sh --explore-ninth
bash switch-port/tests/run_entry_tests.sh --explore-ninth-state-fault
bash switch-port/tests/run_entry_tests.sh --explore-ninth-native-fault
bash switch-port/tests/run_entry_tests.sh --explore-tenth
bash switch-port/tests/run_entry_tests.sh --explore-sort-matrix
bash switch-port/tests/run_entry_tests.sh --explore-tenth-state-fault
bash switch-port/tests/run_entry_tests.sh --explore-tenth-sort-fault
bash switch-port/tests/run_entry_tests.sh --explore-twelfth
bash switch-port/tests/run_entry_tests.sh --explore-twelfth-state-fault
bash switch-port/tests/run_entry_tests.sh --explore-twelfth-key-fault
bash switch-port/tests/run_entry_tests.sh --explore-twelfth-sort-fault
bash switch-port/tests/run_entry_tests.sh --explore-runtime-heap
bash switch-port/tests/run_entry_tests.sh --explore-runtime-state-fault
bash switch-port/tests/run_entry_tests.sh --explore-runtime-lock-fault
bash switch-port/tests/run_entry_tests.sh --explore-runtime-argument-fault
bash switch-port/tests/run_heap_tests.sh
bash switch-port/tests/run_entry_tests.sh --explore-allocated
bash switch-port/tests/run_entry_tests.sh --explore-locale
bash switch-port/tests/run_entry_tests.sh --explore-locale-state-fault
bash switch-port/tests/run_entry_tests.sh --explore-locale-lock-fault
bash switch-port/tests/run_entry_tests.sh --explore-locale-tls-fault
bash switch-port/tests/run_entry_tests.sh --explore-locale-argument-fault
bash switch-port/tests/run_entry_tests.sh --explore-published
bash switch-port/tests/run_entry_tests.sh --explore-stream-locale
bash switch-port/tests/run_entry_tests.sh --explore-first-facet
bash switch-port/tests/run_compare_tests.sh
bash switch-port/tests/run_copy_tests.sh
bash switch-port/tests/run_entry_tests.sh --explore-allocated-state-fault
bash switch-port/tests/run_entry_tests.sh --explore-allocated-native-fault
bash switch-port/tests/run_entry_tests.sh --explore-allocated-provider-fault
bash switch-port/tests/run_entry_tests.sh --explore-allocated-argument-fault
python3 switch-port/tests/test_lifter_links.py
make -C switch-port -j1
python3 switch-port/tools/prepare_sd.py
```

The workspace-local devkitA64/libnx toolchain is used. The generator requires the
verified ELF and PPU lift under `upstream/DemonsSoulsRecomp/work`. Generated
source stays in local build directories. See `entry/README.md` for ABI details.

Version 0.5 passed 191 real-body/accounting/clock checks plus 28 separate fault
fixture checks (219 total), under ASan, UBSan and leak detection, and cross-built
for Switch with strict warnings. It also passed twice on the user's Switch: 35 entries, 1,839 checked accesses,
correct 19.2 MHz counter conversion, advancing time across reload, the expected
first-constructor boundary, and zero-backed-byte cleanup. Tests check real
elapsed time, native microsecond units, long-uptime conversion, clock-provider
failure, timestamp permissions, constructor descriptor faults, TOC and executable
permissions, repeated runs and the older checkpoints.

## Next

Audit constructor fourteen 0x00745248, its tail target 0x00744E88 and
dependencies before advancing the constructor walker. The backend supports free/reuse, but the guest free
override and further malloc callers remain unselected or explicitly unsupported.
The current bridge permits only the twenty-five reviewed operator-new calls, with exact
size/caller/stack/TOC/sequence checks.
Guest
memory allocation must use explicit backing and the same conservative budget
before adding code that requests heap pages. Threading,
files, graphics, audio, input and the rest of the runtime remain substantial work.

Version 0.6 passed 215 real-body/clock/accounting/native-mutex checks plus 40
separate service/fault-fixture checks (255 total), under ASan, UBSan and leak
detection. The Switch cross-build passes strict warnings. It also passed twice on the user's Switch: 53 entries, 2,022 checked accesses,
actual destructor registration, native lock/unlock, eleven mutex records and
complete native/guest cleanup. This executes only a prefix of the first constructor, not a complete
constructor pass, game initialization, graphics or gameplay.

Version 0.7 passed 246 real-body/backend checks and 68 separate service/fault
fixture checks (314 total), with ASan, UBSan and leak detection. Its Switch
cross-build passes strict warnings. It also passed twice on the user's Switch: 68 expected entries, 2,167 checked
accesses, native reader/writer handle creation and CELL_OK, held-manager-mutex
teardown, zero native resources and zero guest backing after cleanup. Tests
verify actual manager fields, native handle output, pool limits, malformed
output/attribute pointers, unsupported IPC attributes, native creation failure,
wrong indirect targets/TOCs and teardown with the manager mutex held.

Version 0.8 passed 270 real-body/backend checks plus 68 service/fault-fixture
checks (338 total), with ASan, UBSan and leak detection. Strict Switch build
passes. It passed on the user's Switch: 72 expected entries, 2,208 checked accesses,
manager publication, the expected static-pool arguments, balanced native locks
and complete native/guest cleanup. One device attempt is recorded for v0.8. New checks cover manager publication,
original unlock with balanced native ownership, the full pool interval and
arguments, invalid unlock TOC and a pool pointer redirected into read-only code.
No allocator initialization or allocation request has executed at this boundary.

Version 0.9 passed 424 host checks with ASan, UBSan and leak detection. Its
Switch cross-build passes with warnings treated as errors (generated upstream
formatting/unused warnings are suppressed). It
completes pool initialization, first allocation, range registration and singleton
publication using the original pinned path. New checks cover exact allocator
memory layout, endian-correct native CAS, native writer ownership, held-lock
cleanup, unsupported wait and fallback paths, and atomic-provider failure.
Version 0.9 passed one attempt on the user's Switch: 95 expected entries,
2,505 checked accesses, pool initialization, first allocation, completed singleton,
balanced native locks and zero native/guest resources after cleanup. This log
contains no reload attempt. Evidence: `analysis/device-startup-v090/loader.log`
and `device-result.json`. See `entry/README.md` for the boundary and the
documented preexisting upstream bootstrap patch.

Version 0.10 reaches 115 function entries and 2,756 checked accesses. The name
registry at 0x01B92A78 owns payload 0x01AD6D90 with one entry and 8,192-byte
capacity. Its record contains the singleton, actual name/empty-string pointers
and length 17. Native mutex calls balance 4/4 and writer calls 1/1; native
resources are cleaned on every stop. The previous 384-byte allocation and
range record are preserved.

The v0.10 Switch build and all 479 host sanitizer checks passed: 341 main,
93 service/fault-fixture, 30 atomic-backend and 15 writer-backend checks.
The installed NRO was read back over USB and matched the 334,021-byte build,
SHA-256 `3b632e6b42aa431823b8b989251871f6b73bf727631a2c1ab981465a09512141`.
Transfer evidence is in `analysis/device-registry-v0100/transfer-verification.json`.
Version 0.10 passed two attempts on the user's Switch, including a fresh reload:
both matched all 115 expected function entries, 2,756 checked accesses, registry
and allocation results, exact stop ABI, balanced locks and zero native resources
after cleanup. The final exit reports zero backed image bytes. The real clock
advanced by 3,721,844 microseconds across reload. Independent log validation
passed 583 checks with no mismatches; evidence is in
`analysis/device-registry-v0100/loader.log` and `device-result.json`.
The verified v0.9 device result above remains a separate earlier milestone.

## Complete first constructor (v0.11)

`ds_entry_run_first_constructor` runs the name registry, key registry and sort,
two linked singleton objects, five allocations and real recursive private
sys_mutex_create (100). The first constructor returns to the CRT walker, which
is stopped before the next descriptor 0x01947390 / code 0x0066D688. There are
176 entries, 3,647 checked accesses, LR 0x016B84F4 and SP 0x0FEFFE00.
`constructor_completed` now distinguishes this complete return from older
checkpoints. The old APIs preserve their earlier stops.

All 583 host checks and the devkitA64/libnx build pass. Tests cover complete
object/free-list/accounting state, fresh reload/reuse refusal, native recursive
registry capacity, invalid kernel syscall spans, unsupported IPC attributes,
provider failure and a fault after real native creation. All native registries
are empty after bounded teardown; guest backing remains image-owned until exit.
Device testing of v0.11 is pending. Details and limitations:
analysis/SWITCH_FIRST_CONSTRUCTOR_STATUS.md.

## Three completed constructors (v0.12)

The first three original constructors now return to the CRT walker. The third
registers ImageProcessSceneEntity metadata, so the name registry has two records
and the opaque-key registry has eight pairs. This does not execute rendering.
Typed dispatch uses 40 verified original indirect callsites in 20 functions;
ambiguous mappings and unknown targets fail explicitly. Automatic and manually
audited dispatch produced identical ELF memory and ABI except actual clock time.

The v0.12 cross-build and checkpoint tests pass; device testing is pending.
See analysis/SWITCH_THREE_CONSTRUCTORS_STATUS.md and INDIRECT_DISPATCH_STATUS.md.
The latest actual Switch result remains v0.10 (two attempts including reload).

## Four completed constructors (v0.13)

The fourth constructor registers DLConsoleAppender metadata, its DLAppender
parent and SetChannel/GetChannel method descriptions. Three type names and
twelve sorted opaque keys are present. Twelve existing-pool requests and four
native recursive mutex creations complete; native locks balance 14/14 and all
native resources are released at the bounded stop. No extra guest arena is
needed. The registered methods are not executed.

All 657 startup/backend/dispatch/lifter checks and 1,315 image-loader checks pass
(1,972 total); all except the nine compiled branch cases use ASan, UBSan and
leak detection. The strict Switch cross-build and package checks pass. The
checkpoint stops before fifth constructor 0x00130D38, after 354 function entries
and 5,937 checked accesses. This passed twice on the Switch, including reload,
with zero native resources and zero guest backing after cleanup. Independent
log validation passed 1,667 checks. Detailed state and build
evidence: analysis/SWITCH_FOUR_CONSTRUCTORS_STATUS.md.

## Six completed constructors (v0.14)

The next two constructors register DsShadowCam and
ShaderContext_DS_FilterEntitySeed metadata and their parents. No camera or
rendering code executes. Five name records and twenty opaque key/object pairs
are validated in sorted order; earlier method data and pool objects remain
intact. Allocation/native-service totals are unchanged from v0.13.

All 686 startup/backend/dispatch/lifter checks pass (677 with ASan, UBSan and
leak detection, plus nine compiled branch cases). The Switch cross-build and
package checks pass. The 415,941-byte NRO was installed over DBI and read back
with exact equality. v0.14 subsequently passed twice on the Switch, including
reload, with 1,947 independent device-log checks passing. The new checkpoint executes 424
function entries and 7,407 checked accesses before constructor seven. Details:
analysis/SWITCH_SIX_CONSTRUCTORS_STATUS.md.

## Nine completed constructors (v0.15)

DsOTCollector, LuaMan and DsMenuDlgSpellInfo metadata now initialize, together
with the counter update, real sentinel stores and one further node/vector. The
registry contains eight type names and thirty-two globally sorted opaque keys.
Five native recursive mutexes are created and cleaned; 16/16 locks balance.
Fourteen requests use the existing pool, with no new guest backing. This does
not execute Lua scripts, menus, rendering or gameplay.

The checkpoint stops before tenth constructor 0x0066A4A8 after 561 function
entries and 10,052 checked accesses. The strict cross-build and package checks
pass. See analysis/SWITCH_NINE_CONSTRUCTORS_STATUS.md for state and validation
evidence. v0.15 passed twice on the Switch, including reload.

All 723 v0.15 startup/backend/dispatch/lifter checks pass, including the older
checkpoints, fresh reload, corrupt descriptors and late failure cleanup. The
432,325-byte NRO was installed and read back byte-for-byte over DBI. Hardware
execution of v0.15 passed twice, including reload, with complete native and
guest cleanup. All 2,495 independent device-log checks pass.

## Constructor ten and larger registry sorting (v0.16)

FileCap metadata now registers through the original larger-range sorter. All
nine name records and 36 key/object pairs are checked. Allocation and native
service totals remain unchanged: 14 pool requests, five recursive mutex creates
and balanced 16/16 locks. No additional guest arena is added. The checkpoint
stops before constructor eleven after 600 entries and 11,007 checked accesses.

All 2,005 host checks pass, including 126 synthetic sort cases with 1,260 checks
against a signed-key/payload oracle. Inputs cover sizes through the existing
512-pair capacity, signed extremes, duplicates, canaries and depth-zero fallback.
The original sorting bodies remain unchanged; fixture seeding is host-only.
The strict Switch cross-build and ZIP CRC/content checks pass. The 440,517-byte
NRO was installed and read back with exact equality. v0.16 passed twice on the
actual Switch, including reload and the larger-sort path, with complete native
and guest cleanup. All 2,653 independent device-log checks pass. Details:
analysis/SWITCH_TEN_CONSTRUCTORS_STATUS.md.


## Locale strings and publication (v0.22)

Original string code constructs `*`, assigns `C`, and publishes the allocated
locale object through the TLS setter, both globals and its public handle.
The697-entry/12,492-access checkpoint validates all128 heap bytes and120 bytes
of the locale aggregate before another40-byte mutex allocation. Three backed
allocations remain40/4/52 bytes; locks balance19/19. Twelve constructors returned;
thirteen remains incomplete. All8,383 sanitizer checks and nine compiled lifter
checks pass; the strict Switch build and ZIP CRC/content checks pass.

v0.22 is installed with exact481,477-byte MTP readback. It passed twice on
the actual Switch, including reload: all 3,121 independent checks pass, with
locale publication, complete traces, exact ABI and zero-backed-byte cleanup.
The post-run binary still matches the sealed candidate. Details and the device test
validator: `../analysis/SWITCH_LOCALE_PUBLISHED_STATUS.md` and
`../analysis/device-locale-published-v0220/validate_device.py`.


## Stream/base locale state (v0.23)

Original runtime code serves another mutex and two locale handles, initializes
stream/base state and begins a facet lookup. Six real allocations now total144
requested/208 rounded bytes in the same64 KiB arena. The747-entry/12,849-access
checkpoint stops before the first24-byte facet allocation. Twelve constructors
returned;13 and the stream constructor remain incomplete. One held native guard
is cleaned at stop. All791 sanitizer checks and nine lifter checks pass,along
with the strict Switch build and ZIP CRC/content validation.

v0.23 passed twice on the actual Switch, including reload: all3,353 independent
checks pass, with complete traces/ABI, six allocations, stream/base locale state,
held-lock cleanup and zero guest backing at exit. The post-run binary matches
the sealed497,861-byte candidate. MTP is released.
Detail:`../analysis/SWITCH_STREAM_LOCALE_STATUS.md`.


## First locale facet (v0.24)

Original code constructs the first facet, saves/restores locale state and
registers its cleanup callback and list node. Eight real allocations now total
176 requested/256 rounded bytes in the same64 KiB arena. All25/25 native locks
release normally. The799-entry/13,238-access checkpoint stops before invoking
virtual009E96A0. Twelve constructors returned;13 and stream construction are
incomplete. All17,145 sanitizer checks and nine lifter checks pass,along with
the strict Switch build and ZIP CRC/content checks.

v0.24 passed twice on the actual Switch, including reload: all3,591 independent
checks pass, including full traces/ABI, eight allocations, first facet/cache/
cleanup-node state and zero guest backing at exit. Post-run installed binary
matches the sealed506,053-byte candidate; MTP is released.
Detail:`../analysis/SWITCH_FIRST_FACET_STATUS.md`.

## First narrow stream (v0.25)

Original facet virtual009E96A0 returns the space fill character; original
01684D70 registers the first narrow stream in slot one. It returns before
the second narrow stream reaches its next four-byte allocation. The bounded
stop is810 entries/13,312 accesses with eight allocations and26/26 locks.
Constructor thirteen remains incomplete; there is no stream I/O or gameplay.
Current validation and installation:analysis/SWITCH_FIRST_STREAM_STATUS.md.

## Narrow streams complete (v0.26)

All four original narrow streams register in slots one through four and
01686CE8 returns. Stop37 precedes wide initializer0168BC68, at966 entries /
14,378 accesses. Thirteen allocations total232 requested /368 rounded bytes
in the same64 KiB arena; locale refs9,15 CRT mutexes,42/42 locks.
All215 original body pins and63 indirect markers remain unchanged.
Validation and hardware status:analysis/SWITCH_NARROW_STREAMS_STATUS.md.

## Wide streams and constructor thirteen (v0.27)

All eight original stream objects register, original character conversion runs,
and constructor thirteen returns. Stop38 precedes constructor fourteen at
1,265 entries/16,590 accesses. Production pins223 full bodies with65 indirect
markers. Twenty-five allocation contexts total420 requested/672 rounded bytes;
18 CRT mutexes initialize and66/66 locks release. CRT/trace bounds are now
32/2048. Validation/device status:analysis/SWITCH_WIDE_STREAMS_STATUS.md.
