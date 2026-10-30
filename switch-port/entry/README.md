# Real translated CRT startup checkpoints

The generator hash-pins 223 complete function bodies from the verified local
BLUS30443 1.00 lift. Production adds verified original indirect-callsite markers;
unannotated bodies remain available for the older regression checkpoints. Body hashes and source chunks are pinned in
`generate_entry.py`. The local upstream lift includes a NULL-pool bootstrap patch
in 0x00AC8078; the port does not implement its ds_malloc fallback. The accepted
nonzero-pool path never takes it. Generated code belongs in local build directories.

| Address | Role |
| --- | --- |
| 0x000124C0 | Process entry |
| 0x000125F0 | CRT initialization |
| 0x016BB3E4 | sys_initialize_tls import |
| 0x00011EA0 | Library TOC-switching thunk |
| 0x0167ACE0 | Default-heap initialization wrapper |
| 0x0167A9C8 | Heap initializer |
| 0x016BB2C4 | sys_lwmutex_create import |

Additional v0.4 bodies: 0x00010D60, 0x01676368, 0x000120C0, 0x0166FA10,
0x01676258, 0x00010D50, 0x01675CB0 and 0x016BB404. These add CRT/library
thunks, mutex/stream initialization, descriptor-table initialization and the
time import stub. All original body hashes are pinned.

## APIs and boundaries

- `ds_entry_run`: stops before sys_initialize_tls; three function entries and
  thirteen guest accesses. Preserves the original pre-service checkpoint.
- `ds_entry_run_tls`: implements main-thread TLS and stops with its return
  registers set, before executing further CRT instructions. 1,093 guest accesses.
- `ds_entry_run_crt`: continues TLS return, compacts argv/envp, switches library
  TOC, creates the first unlocked mutex record, and stops before LV2 syscall 352.
  Seven function entries. This API preserves the original pre-query checkpoint.
- `ds_entry_run_memory`: executes syscall 352, returns CELL_OK, then continues
  through ten mutex records and stream/descriptor tables. Stops before the first
  time import (NID 0x8461E528). Fifteen unique bodies, 31 entries in exact order.

All modes require independently verified input hashes. Unknown later functions,
imports and syscalls are explicit failure boundaries. A successful result must
match the expected function sequence, ABI registers and selected stop reason.
`callee` is the last function entry breadcrumb, not the currently unwound frame.

## Memory and ABI

The adapter creates/reuses bounded anonymous stack (1 MiB at 0x0FE00000) and
initial TLS (64 KiB at 0x10F00000) arenas. sys_initialize_tls adds a 36 KiB arena
at 0x0E000000, copies the four-byte template and zeroes the tail, sets
r13=0x0E007000 and r3=0. This follows the pinned desktop HLE layout.

CRT mode adds 4 KiB at 0x10000000. argv[0] is the 64-bit big-endian pointer to
`/dev_bdvd/PS3_GAME/USRDIR/EBOOT.BIN` at arena+0x20, followed by null argv and
envp slots. r3/r4/r5/r6 are argc=1, argv, envp, envc=0. Real CRT instructions
compact the pointers in place, including terminators. This avoids the desktop
bootstrap's scratch address 0x00B00000, which lies in the verified RX segment.

The imported HLE stub omits the original glink `std r2,40(r1)`. Before returning
from either implemented service in CRT mode, the adapter saves r2 to that ABI
slot, matching the pinned runtime's `ppu_hle.cpp` TOC guard. The subsequent
original `ld r2,40(r1)` then restores the actual caller TOC. No invalid access
is silently redirected or given a fabricated zero.

The mutex service initializes each guest record (owner/waiter/recursion/
queue fields zero, attribute protocol copied), matching the pinned HLE layout.
It validates both spans before writing. Older pre-constructor modes initialize
guest records only; constructor/startup modes add the bounded native
synchronization described below. Guest secondary-thread operations remain
unsupported.

All allocations remain owned by the image, including on failure. Caller must
invoke `ds_image_destroy`. Older pre-constructor modes can reset and reuse exact
owned arenas; constructor/startup attempts require a freshly reloaded image.
Incompatible overlaps and insufficient budgets fail.

## Control flow and validation

Single-threaded, nonreentrant `setjmp`/`longjmp` context; lifted bodies use trivial
automatic objects and the desktop RAII shadow stack is disabled. Function entry
hooks trace at most 1024 entries; guest access count is exact while at most
32 trace entries are stored. Additional functions with nontrivial destructors
need an unwind audit before inclusion.

Trampoline storage preserves the lift's erased function-pointer representation,
but both drain sites cast back to the original `void(ppu_context*)` type before
invoking it. This fixes a UBSan function-type violation without changing bodies.

```sh
bash switch-port/tests/run_entry_tests.sh
```

117 real-body checks, 6 unsupported-import fixture checks and 5 unsupported-syscall
fixture checks passed under ASan, UBSan and leak detection. They cover TLS bytes,
zero fill, saved TOCs, argv conversion, actual mutex fields, memory budgets,
repeated runs, overlap and permission rejection. Separate changed-NID/syscall
fixtures never enter production generation or packaging. Version 0.2's TLS
checkpoint passed on device. The extended CRT version 0.3 also passed twice on
the user's Switch: seven functions, 1,131 checked accesses, pre-syscall-352 stop,
and cleanup with zero guest backing. See analysis/device-crt-v030/loader.log.

## Bounded memory-size service (v0.4)

`ds_image_memory_info` reports a 1 MiB-granular configured total and available
allowance. It charges every image arena (ELF, stack, both TLS areas, argv and any
other backing). Totals clamp below 4 GiB; round-down prevents overpromising;
overcharged accounting is rejected without unsigned subtraction wrap. An exhausted
or sub-page allowance reports zero available. This is not an OS memory-capacity
query and does not guarantee later host allocation success.

The syscall validates all eight output bytes before either 32-bit big-endian
write, records the output address/values and exact backing charge, and sets r3=0.
Its default output is 48 MiB total and 17 MiB available. It changes no mappings or
allocations. Old pre-query APIs retain their previous behavior.

The default heap threshold is an actual read-only zero in this image, so the
normal path returns from its initializer and runs the following CRT initializers.
A test-only redirected threshold exercises the low-memory warning branch; the
next tty syscall 403 fails explicitly. No alternate path is forced to return
success. Production clock calls, memory allocation, lock/wait and threads remain
unsupported. v0.4 passed twice on the user's Switch: 31 expected entries, 1,816 checked
accesses, ten expected mutex records, completed memory query, deliberate stop
before the clock import, and cleanup with zero guest backing. See
analysis/device-memory-v040/loader.log and device-result.json.

Final v0.4 host run: 153 real-body/accounting checks plus 23 separate
service/fault-fixture checks passed (176 total) under ASan, UBSan and leak
detection. Switch devkitA64/libnx cross-build passed with strict warnings.

## Monotonic time and constructor enumeration (v0.5)

`ds_entry_run_clock` extends the real path through sys_time_get_system_time.
Four more unmodified bodies are pinned: 0x00010210 (initialization wrapper),
0x00012878 (optional initialization hook), 0x00010200 (library thunk), and
0x016B84B0 (static constructor walker). The checkpoint records 35 entries over
19 distinct bodies. It stops before the first constructor at code 0x007CF848,
OPD 0x01951EA8, TOC 0x019B5040. The optional hook is skipped by the real image's
initial state; unknown indirect call paths remain explicit failures.

`ds_clock_now` reads the Switch architectural counter/frequency via libnx or
CLOCK_MONOTONIC on Linux. A 128-bit intermediate converts raw ticks to truncated
microseconds and rejects invalid frequencies or signed-64 range overflow. There
is no wall clock, synthetic increment or per-run epoch reset. Provider failure
stops explicitly without changing guest timestamp storage. The import saves
the caller TOC slot, returns microseconds in r3, and the original guest store
writes them to 0x01D2B5C8 before entering constructor enumeration.

The new indirect-call adapter validates the original walker's resolved descriptor,
CTR/code and r2/TOC against the known first constructor and executable backing.
It does not invoke it or implement arbitrary indirect dispatch. The acceptance
predicate also verifies the timestamp global, all 35 entries, stack/LR/TOC/TLS,
completed memory query and ten mutex records. No new guest arenas are allocated.

Host validation: 191 real-body/clock/accounting checks plus 28 separate fault
fixture checks (219 total), ASan/UBSan/leak detection; Switch cross-build with
strict warnings. Clock-provider failure is injected only through a host linker
wrapper; that fixture never enters Switch packaging. v0.5 passed twice on the user's Switch: 35 entries, 1,839 checked accesses,
correct hardware-counter conversion, advancing time across reload, the expected
constructor boundary and cleanup with zero guest backing. Evidence is in
analysis/device-clock-v050/loader.log and device-result.json.

## First constructor prefix and native synchronization (v0.6)

`ds_entry_run_constructor` executes the first constructor (0x007CF848) through
its actual destructor-registration path. Seventeen more pinned original bodies
bring the local slice to 36 bodies and 53 function entries. The explicit stop
is before singleton initializer 0x00AE7768, LR 0x007CEF28, SP 0x0FEFFCA0, with
the main TOC. The first constructor is started but not completed.

Two whitelisted indirect calls validate the real descriptor/code/TOC and
executable backing. The constructor call restores LR 0x016B84F4 from original
bctrl at 0x016B84F0; the virtual method at 0x00A662C0 restores LR 0x00A667D4
from bctrl at 0x00A667D0. General indirect targets are rejected. Original
instruction bodies remain unmodified; only bounded dispatch adapters are added.

The thread-ID import (0x350D454E) writes main guest thread identity 1 into a
checked 64-bit output. Mutex lock (0x1573DC3F) and unlock (0x1BC200F4) validate
registered addresses, guest owner/recursion fields, caller TOC slots and zero
wait timeout. They use libnx Mutex on Switch and pthread mutexes on Linux via
a 16-record pool. The supported path is one uncontended acquisition/release by
the single active host thread; unknown owners, waits, contention and native
recursion fail explicitly without synthetic successful returns. Guest scheduling
and cross-thread ownership transfer remain unsupported.

The real atexit code changes free callback slots 80 -> 79 and writes OPD
0x01967920 into slot 0x01CE58E8. It locks/unlocks CRT mutex 0x01CE5940; the
constructor then creates object mutex 0x01B92D00, bringing the total to eleven.
No extra guest arenas are allocated. The acceptance predicate checks the exact
53-entry sequence, ABI, atexit memory effects, service counts, and no held locks.
Native resources are destroyed on every nonlocal stop, including guest faults.

Partially initialized guest data cannot be treated as a fresh process. A per-image
constructor-attempt flag rejects repeat execution until image destroy/reload;
the loader's A button already reloads. No context is resumable after a checkpoint.
The older pre-constructor checkpoints retain their previous behavior.

Validation: 215 real-body/backend checks plus 40 separate service/fault fixtures
(255 total), ASan/UBSan/leak detection; strict Switch cross-build. Tests include
wrong owners, duplicate/invalid native records, contended/reentrant refusal,
cleanup with a held lock, unsupported wait timeout, actual atexit memory effects
and fresh-image reload. Production code contains no injected fault paths.
Version 0.6 passed twice on the user's Switch with 53 expected entries,
2,022 checked accesses, actual destructor registration and native lock/unlock,
eleven mutex records, the deliberate singleton boundary and full cleanup.
Evidence: analysis/device-constructor-v060/loader.log and device-result.json.
The first constructor remains incomplete.

## Singleton / reader-writer creation checkpoint (v0.7)

`ds_entry_run_singleton` extends execution through 0x00AE7768, 0x00A35DB0 and
0x00A35780 into actual manager construction. Thirteen additional pinned bodies
bring the slice to 49 original bodies and 68 function entries. The old pre-entry
singleton checkpoint is preserved by the DS_FRAME hook before counting or
executing that function in the older constructor mode.

Two additional virtual calls validate their descriptors/code/TOC/executable
backing: the manager lock method (OPD 0x019629E8, code 0x00A664E0, return
0x00A35A18) and reader/writer state query (OPD 0x01964BF0, code 0x00AB5CC0,
return 0x00AB63B8). Unknown targets and mismatched TOCs fail explicitly.

Singleton address: 0x01B4C6E8. Manager address: 0x01B91128. Original instructions
initialize its self pointer and derived vtable 0x01872060 at reader/writer object
0x01B911A8. The original state query sees the -1 handle and calls syscall 120,
which the pinned LV2 table identifies as sys_rwlock_create.

The service validates a four-byte writable output and the full 32-byte readable
attribute structure. It accepts the observed protocol=2, private pshared=0x200,
zero IPC key/flags, and records all raw words. An eight-entry registry initializes
real libnx RwLock objects (pthread_rwlock on Linux), returns an opaque guest handle
and writes it big-endian at 0x01B911AC. r3 is set to CELL_OK only after native
creation and the guest write. The checkpoint stops before the original caller
processes that return; singleton and first constructor are still incomplete.
Reader/writer acquisitions, waiter priority behavior and IPC are unsupported.

A second native mutex lock protects manager construction. At the stop, two locks
and one unlock have occurred and one native manager mutex is held. Teardown
releases it and destroys all eleven native mutex records and the new reader/writer
object. No extra guest arenas are allocated. Restart requires image reload.

Validation: 246 real-body/backend checks plus 68 separate service/fault checks
(314 total), ASan/UBSan/leak detection; strict Switch cross-build. Fixtures cover
null/crossing outputs, malformed attributes, unsupported shared scope, native
reader/writer creation failure and indirect-target/TOC corruption. Failed creation
leaves the guest -1 handle unchanged and cleans all native resources. v0.7 passed twice on the user's Switch: 68 expected entries, 2,167 checked
accesses, native handle 1/CELL_OK, the expected service boundary and complete
held-mutex/native-reader-writer/guest cleanup. Evidence:
analysis/device-singleton-v070/loader.log and device-result.json.

ABI references checked on 2026-10-23: [RPCS3 rwlock attributes](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/Cell/lv2/sys_rwlock.h),
[synchronization constants](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/Cell/lv2/sys_sync.h),
and [rwlock creation](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/Cell/lv2/sys_rwlock.cpp).
The pinned local runtime's LV2 table supplies syscall numbering; local libnx
headers supply native API declarations. The older desktop rwlock name offset
is not copied: the full attribute layout is checked against the primary reference.

## Manager publication and static-pool handoff (v0.8)

`ds_entry_run_manager` returns from the real rwlock service into its original
caller, then runs the remaining manager-object initialization, publishes it,
and invokes the original unlock method. Three more pinned bodies (0x00967998,
0x00A662C8 and 0x0002F0B8) bring the slice to 52 bodies and 72 entries.
The new virtual unlock validates OPD 0x019629D8, target 0x00A662C8, main TOC,
executable backing and incoming LR 0x00A35B7C, then sets the actual bctrl return
LR 0x00A35A48. Native mutex acquisition/release is balanced at this stop.

The original publication writes 0x01B91128 to 0x01B9111C; its control word at
manager+0x88 is zeroed by original code. The configuration getter produces
pool base 0x01AD6B78 and size 0x60000 (384 KiB). The adapter validates the entire
interval as readable/writable existing ELF backing. No extra guest arena is
allocated. It stops before executing func_00B25B58, with manager in r3, pool
base/size in r4/r5, LR 0x00A3585C and SP 0x0FEFFAB0. The outer singleton and
first constructor remain incomplete; pool initialization has not executed.

The old APIs keep their previous stops. New attempts require fresh image reload,
and all native mutex/rwlock resources are torn down on every stop. Validation:
270 real-body/backend checks plus 68 fault fixtures, 338 total, ASan/UBSan/leak
detection and strict Switch cross-build. New tests cover actual publication,
configuration arguments, the full pool range, balanced native unlock, corrupted
unlock TOC and a pool redirected into RX memory. v0.8 passed one device attempt: 72 expected entries, 2,208 checked accesses,
manager publication, balanced native mutex ownership, expected static-pool ABI
and zero native/guest resources after teardown. Evidence:
analysis/device-manager-v080/loader.log and device-result.json.

## Pool, first allocation and completed singleton (v0.9)

`ds_entry_run_startup` adds thirteen pinned bodies, bringing the slice to 65
bodies and 95 function entries. It runs pool initializer 0x00B25B58 and the
original allocator helpers against the existing 384 KiB ELF-backed pool.
It checks initial pool headers before the real compare/exchange loop at
0x00967AA0. The native CAS validates a complete readable/writable aligned word,
converts expected/desired values to big-endian backing, and returns the actual
comparison outcome. This supports the immediate reservation loop of one guest
thread; general PPC reservation invalidation, DMA and concurrent guest execution
remain unsupported.

The first request is 0x180 bytes with alignment exponent 4. Original code splits
the free chunk and returns 0x01AD6C00. Tests independently check split headers,
free lists, remaining size, vector capacity and the three-word range tuple.
No new guest backing is allocated. Audited virtual lock/unlock calls restore
LR 0x00B26C68 / 0x00B26CC4; the range interface method restores 0x00AE77E8.
Syscalls 125/127 use the existing native rwlock for an uncontended writer
acquire/release. Nonzero timeout, invalid ownership and contention stop explicitly.
Reader operations and wait queues are unsupported.

The original code sets the TLS tag, registers the range, finishes singleton
0x01B4C6E8 and publishes it at 0x01B4C6E0. The stop is before initializer
0x00AE4D38, LR 0x007CF75C, SP 0x0FEFFD40, r3=singleton, main TOC preserved.
The first outer constructor remains incomplete. Twelve native mutex records
have been created; actual locks/unlocks balance 3/3, native writers 1/1. All
native resources are destroyed at every bounded stop; guest backing is owned
until image destruction. A watchdog stops excessive guest accesses.

The pinned 0x00AC8078 body contains a preexisting upstream NULL-pool bootstrap
patch. Provenance names it explicitly. Its ds_malloc fallback is unsupported,
counts the attempted call, and stops; the accepted nonzero-pool path records
zero fallback calls. No fallback implementation enters the port.

424 host checks passed with ASan, UBSan and leak detection: 291 main checks,
88 service/fault-fixture checks, 30 atomic checks and 15 native-writer checks.
New fixtures cover a guest fault with a native writer held, unsupported writer
wait, forced unsupported bootstrap fallback and atomic-provider failure. Device
execution of v0.9 passed one attempt: 95 expected entries, 2,505 checked accesses,
completed pool/allocation/singleton state, balanced native ownership and zero
native/guest resources after cleanup. No hardware reload attempt is present.
Evidence: analysis/device-startup-v090/loader.log and device-result.json.
Runtime acceptance also verifies the final pool
split, capacity, range tuple, singleton fields and TLS tag. The new range-method
adapter checks its incoming LR as well as descriptor/code/TOC; a corrupted-TOC
test verifies refusal and teardown. Switch cross-build passes with warnings
treated as errors, suppressing generated upstream formatting/unused warnings.

## First name-registry insertion (v0.10)

`ds_entry_run_registry` continues through initializer 0x00AE4D38, lazily
constructs its first name registry, performs a second pooled allocation and
inserts the real singleton record. Nine more pinned complete bodies bring the
slice to 74 bodies and 115 entries, with 2,756 checked accesses. The old
pre-0x00AE4D38 checkpoint is preserved by its entry hook before counting the
body in the older startup mode.

The registry container is 0x01B92A78. It retains allocator interface
0x01B91190 and obtains 0x2000 bytes at payload 0x01AD6D90. Its begin/end/capacity
are payload, payload+0x10 and payload+0x2000. The first 16-byte tuple is
`{singleton=0x01B4C6E8, name=0x016F8410, empty=0x016F8320, length=17}`. The
original translated strlen body at 0x016693E0 reads through checked guest
accessors; no host strlen substitution or synthetic result is used.

The merged 0x00010BA0 body is retained completely and pinned, including its
unreachable later trampolines. Only its first trampoline to strlen executes
on this accepted path; later unsupported references remain strict stops.
Audited indirect adapters cover the cached-manager status call, both singleton
field getters and interface allocation wrapper, including its native lock and
unlock calls. Each validates descriptor/code/TOC/executable backing and restores
the original bctrl LR. The expected LRs are 0x00AE21DC, 0x00ADEBF8, 0x00ADEC20,
0x00AE4E18, 0x00B276AC and 0x00B27708 respectively.

The second allocation splits the remaining existing pool; no guest backing
arena is added. The first 384-byte allocation at 0x01AD6C00 and its registered
range remain intact. Native mutex lock/unlock calls balance 4/4, writer calls
1/1, and the single initial CAS is unchanged. All native resources are cleaned
after the bounded stop; image-owned backing is freed on image destruction.

The stop is before separate callback-registry initializer 0x00AE4490, stop=18,
LR 0x00AE50E0, SP 0x0FEFFC30, main TOC, r3=0x01AD6DA0. That r3 is the preceding
copy helper's return, not an argument consumed by the next initializer. The
singleton is complete; the first outer constructor and callback registry are
not complete. No subsequent callback insertion or sorting is claimed.

The v0.10 Switch build and all 479 host sanitizer checks passed: 341 main,
93 service/fault-fixture, 30 atomic-backend and 15 writer-backend checks.
The installed NRO was read back over USB and matched the 334,021-byte build,
SHA-256 `3b632e6b42aa431823b8b989251871f6b73bf727631a2c1ab981465a09512141`.
The transfer record is `analysis/device-registry-v0100/transfer-verification.json`.
Version 0.10 passed two hardware attempts, including a fresh reload. Both
115-entry traces match the earlier v0.9 prefix of 95 entries and the audited
20-entry registry suffix; both report 2,756 accesses and the exact stop ABI
above, including r4=0x0FEFFCB0, r5=r6=0x10. Native mutex/writer counts balance
4/4 and 1/1, with zero held or live resources after each bounded stop. Final
exit reports zero backed guest bytes. Clock conversions are exact at 19.2 MHz;
the second attempt advances by 3,721,844 microseconds. Independent validation
passed 583 checks with no mismatches, recorded in
`analysis/device-registry-v0100/device-result.json` alongside the source log.
The successful v0.9 device checkpoint above is retained as historical evidence.

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

## Verified callsite dispatch and three constructors (v0.12)

`--annotate-indirect-calls` matches ordered call statements with original bctrl
instructions in the verified function ranges. Provenance records all 40 sites
in 20 functions and identifies that annotated output differs from source bodies.
The typed dispatcher checks the original opcode, executable target, backed OPD
and main TOC; no unknown native pointer can be invoked. Ambiguous mappings,
unsupported link forms and unknown targets remain explicit boundaries.

`ds_entry_run_exploration` runs to an explicit constructor limit or unsupported
boundary and does not claim boot success. `ds_entry_run_three_constructors`
validates the 213-entry, three-return checkpoint, actual parent objects, two name
records, eight sorted key/object pairs and native cleanup before returning true.
Stop is before fourth constructor OPD 0x0195FFE0 / code 0x009E54C8, with original
CRT SP 0x0FEFFE00 and LR 0x016B84F4. Full guest-image comparison against manual
dispatch ignores only the actual per-run clock timestamp. A corrupted annotated
PC is refused before constructor execution. Three-constructor tests cover fresh
reload, replay refusal and a corrupted third-object getter TOC. The lifter's
indirect linked branches now restore LR correctly for future regeneration;
nine compiled branch-execution cases pass. Existing source chunks are retained.

## Fourth constructor and method metadata (v0.13)

`ds_entry_run_four_constructors` runs the annotated checked path to constructor
five (OPD 0x01923818 / code 0x00130D38). It validates four completed returns,
354 entries, three sorted name records, twelve opaque key/object pairs and the
DLConsoleAppender method records. The additional 56-byte nodes and 16-byte
vectors use the existing pool; native handles 0x1001 through 0x1004 are actually
created and all destroyed on bounded teardown. The registered methods are data
only at this checkpoint. Error/fallback dependencies remain explicit stops.

120 source bodies and 55 original indirect-call sites in 24 functions are
pinned. `--explore-fourth` checks fresh reload, reuse refusal and descriptor
failure after handle 0x1002. `--explore-fourth-fault` injects a guest-memory fault
after actual handle 0x1004, verifying complete native teardown. The earlier
three-constructor API and automatic/manual memory equivalence continue to pass.

## Six completed constructors (v0.14)

`ds_entry_run_six_constructors` returns stop 24 before OPD 0x01920778 / code
0x000BF6E0, after six original returns, 424 entries and 7,407 checked accesses.
The new DsShadowCam and ShaderContext_DS_FilterEntitySeed records and parent
objects consume existing backing and no new allocations or native services.
Five type-name records and twenty key/object pairs are validated in their full
sorted order. No camera behavior or rendering executes.

138 original bodies are pinned; original indirect sites remain 55 in 24
functions. Shared registry validation preserves the three/four-constructor
APIs. `--explore-sixth` covers fresh reload, replay refusal, two corrupt TOCs
and a valid executable but unselected getter. `--explore-sixth-state-fault`
corrupts an opaque-key object after six returns and requires final validation
to reject it while leaving all native registries empty.

## Nine completed constructors (v0.15)

`ds_entry_run_nine_constructors` returns stop 25 before OPD 0x01947270 /
code 0x0066A4A8. Nine constructors execute 561 entries and 10,052 accesses.
DsOTCollector increments the original 64-bit counter; LuaMan registers only its
type; DsMenuDlgSpellInfo writes its sentinel halfwords and creates another
metadata node/vector and native recursive mutex. Totals are 14 pool requests,
five native heavyweight creates, 16/16 locks and 1/1 writer. No extra guest
arena is added.

All eight name records and 32 sorted opaque key/object pairs are checked,
together with parent links, prior method objects and the fifth actual native
handle. The old three-, four- and six-constructor APIs retain their checkpoints.
New tests cover fresh reload, reuse refusal, three invalid getter TOCs, corrupt
final registry data after nine returns, and a guest fault after real native
handle 0x1005. The last two paths must clear every native registry. Selected
native functions remain typed and hash-pinned; larger-sort dependencies are
not selected.

## Ten constructors and larger sorting (v0.16)

`ds_entry_run_ten_constructors` returns stop 26 before OPD 0x01943CD0 / code
0x005FD720. The actual path executes 600 entries and 11,007 accesses, registers
FileCap and its parent, and sorts 36 opaque keys through the original ADF198
partition helper. All nine name records and 36 pairs are checked. Existing pool
and native-service totals are unchanged.

Checkpoint constants now use explicit profiles for constructors 3, 4, 6, 9 and
10, including checked-access counts. The isolated `--explore-sort-matrix`
fixture seeds the tenth outer-sort call, runs original sorting bodies unchanged
and compares signed order and key/payload preservation against a host oracle.
All 126 cases pass 1,260 checks through the real 512-pair allocation capacity.
The seeding callback and test source are never linked into the Switch build.
New faults cover a corrupt FileCap TOC, null partition input and corruption of
the last sorted object after all ten returns. All release native resources.


## Eleven-constructor checkpoint (v0.17)

`ds_entry_run_eleven_constructors` completes eleven original constructors in
603 entries / 11,044 checked accesses, then stops at OPD 0x01935910 / code
0x003F46B0. The new wrapper 0x005FD720 and initializer 0x005FD628 use checked
scalar memory and the already selected composite initializer 0x00DBA100.
All ten static object words at 0x01B48C44..0x01B48C68 and the final ABI are
validated alongside every earlier metadata object and both registries. There
are no additional allocations or native service calls. Older APIs preserve
their checkpoints. See analysis/SWITCH_ELEVEN_CONSTRUCTORS_STATUS.md.

## Twelve-constructor checkpoint (v0.18)

`ds_entry_run_twelve_constructors` checks 641 entries / 12,134 checked accesses
and stops before constructor thirteen, OPD 0x019A3EA0 / code 0x015166F0.
Its runtime TOC 0x019D3BAC is validated only for that exact descriptor, code,
and twelve completed returns. Other unexpected TOCs still reject immediately.

Nine reviewed original bodies add ImageProcessDrawEntitySeed registration:
wrapper 003F46B0, initializer 003F44E0, six small scalar accessors 003F3F10,
003F3F18, 003F3F30, 003F3F48, 003F3F60, 003F3F78, and parent getter 00CE3DF0.
The name length is 26 bytes. The new root 01B42878 links to parent 01BB6060.
All ten names and forty sorted key/object pairs are checked; the original
larger-sort partition now executes twice, at 36 and 40 pairs. Native totals,
pool free bytes and backed arenas match the eleven-constructor stage.

`--annotate-indirect-calls` controls verified return-PC annotations only.
Experimental selections require the independent `--include-frontier` option.
Production uses 186 approved bodies; no constructor thirteen body is selected.
See analysis/SWITCH_TWELVE_CONSTRUCTORS_STATUS.md for tests and evidence.

## Runtime allocation boundary (v0.19)

`ds_entry_run_runtime_heap` completes twelve returns and executes the original
constructor-thirteen prefix through locale counter/vtable initialization. It
validates 652 entries / 12,206 checked accesses and returns stop29 before
operator new 0x0168ABD0 with r3=40, LR=0x016A051C, SP=0x0FEFFB60,
TOC=0x019D3BAC. r4/r5 are zero and r6 remains 0x01AD8EE0.

Four newly pinned bodies bring production to190:015166F0,01516590 (chunk010),
01686CE8,016A0500 (chunk011). The full500-line locale initializer is retained;
its later dependencies are strict stops. Two verified original bctrl sites
in that body bring annotations to60 sites in26 functions. Its actual path
uses an already-created CRT mutex, balances17/17 locks, sets counter01AD6020
to1 and vtable01CE64E8 to018C1BF0, and leaves allocation output01CE64EC zero.
Wide initialization and five following64-bit counters remain untouched.

The checkpoint validates all prior metadata with the same state validator.
Its mode is explicit; ordinary exploration still rejects the unselected
allocator. Earlier checkpoint APIs retain their exact stops. A held-runtime-
mutex fault must release that native ownership during teardown.

Constructor thirteen has not returned. No allocation is served by this stage.
The inspected operator-new route reaches preexisting host override0167AC88;
future work needs a guest heap with explicit backing/budget/free semantics.

## Locale object initialization (v0.21)

`ds_entry_run_locale_object` adds four pinned original bodies:016896C0,
016891F0,016B7F30 and01689180. Production now selects196 bodies; indirect
annotations remain61 sites in27 bodies. The real TLS slot0E000430 is null.
The getter acquires lock zero, rechecks TLS and requests52 bytes at LR016892DC.
This third malloc alone is additionally permitted in the new mode; older
checkpoints retain their counts and boundaries via a hook before function entry.

The resulting object at60000040 has vtable018C7DE0,reference count1,zero scalar
fields and empty small-string storage with capacity15. All128 bytes of the
three allocations' rounded span are checked. The original TLS publication and
public handle assignment have not executed. Stop32 precedes string thunk
01676088,after669 entries/12,314 accesses,r3=60000058,r4=017041B8,
LR016891DC,SP0FEFFA30,TOC019D3BAC. Twelve constructors have returned.

Heap accounting is96 requested/128 live-rounded bytes in the same64 KiB arena.
There are18 native acquisitions/17 guest releases,three thread-ID calls and
one held lock at stop. Cleanup releases it and clears all native and heap
metadata; fresh repetitions and failure injection verify this behavior.
Detail:[locale checkpoint audit](../../analysis/SWITCH_LOCALE_OBJECT_STATUS.md).

## Backed runtime allocations (v0.20)

`ds_entry_run_runtime_allocated` executes the pinned operator-new0168ABD0 and
the preexisting malloc override0167AC88, which now calls the native guest heap
only for two audited calls. The unsupported NULL-pool bootstrap path remains
separate. Production includes192 pinned bodies, with61 bctrl sites in27 bodies.

It serves40 bytes at60000000, then4 bytes at60000030. Both share one64 KiB arena
within the configured image cap; rounded live bytes64, requested bytes44. The
original016A0500 caller publishes60000000 at01CE64EC and uses its first40 bytes
to initialize CRT mutex13 at60000010, with protocol2 at60000018.

Stop30 is before unselected locale consumer016896C0, after658 entries/12,247
checked accesses. ABI:r3=60000030,r4=0176F780,r5=0,r6=01AD8EE0,
LR=01686E14,SP=0FEFFBF0,TOC=019D3BAC. Twelve constructors have returned;
thirteen remains incomplete. All prior metadata and new64-byte allocation
contents are validated; native locks remain17/17 and both native/heap metadata
registries clear after stop/failure. Guest backing remains image-owned until
image destruction, keeping the checkpoint inspectable and fully budgeted.

`ds_heap.c` has host-side metadata, first-fit aligned splitting and coalescing
within each owned arena. Zero-size requests own a16-byte minimum allocation.
The API accepts power-of-two alignment16..65536; overflow and invalid inputs
fail without pointer truncation. Exact-owner free rejects interior,double and
foreign pointers; reclaimed blocks are reused without releasing their physical
charge. Shutdown can release exact owned anonymous segments and budget through
`ds_image_remove_zero_region`; ELF or mismatched backing cannot be released.

Limits:8 MiB heap backing cap,8 arenas,1024 block descriptors,4 KiB page units,
initial64 KiB arena and bounded geometric growth. These are independent
diagnostic limits; neither the heap cap nor process memory guarantees allocation
success. Native memory failure is explicit. The current runtime bridge stops
with heap failure on OOM; C++ new-handler/exception paths remain unimplemented.


## Locale strings and publication (v0.22)

`ds_entry_run_locale_published` executes original string construction and assigns
`C`, publishing object60000040 in TLS0E000430, handle60000030 and both global
slots01CE672C/01CE6730. Refcount3,flags63,string size1/capacity15,all128 heap
bytes and the120-byte locale aggregate are validated. Seven additional original
bodies bring production to203; both four-instruction cross-TOC thunks are verified
against the ELF and retain unreachable neighboring source sequences unchanged.
Indirect annotations remain61 in27 bodies. The404-line original copy body passes
7,681 oracle/ABI checks across lengths, alignments and address orders.

Stop33 precedes another40-byte operator-new request at0168ABD0,after697 entries/
12,492 accesses,LR016A051C,SP0FEFFB60,TOC019D3BAC,r3=40,r4=01AD58D0,
r5=01CE6510,r6=01CE6500. Twelve constructors have returned;13 is incomplete.
Three40/4/52-byte allocations and backing stay unchanged. Thread-ID calls5,
locks19/19,no held native lock at stop; all cleanup passes. The v0.21 and earlier
APIs retain their original stops/counts. Further allocation callers,string
growth,free,new-handler,exception and memmove paths remain unsupported.
See analysis/SWITCH_LOCALE_PUBLISHED_STATUS.md for validation and installation.


## Stream/base locale state (v0.23)

`ds_entry_run_stream_locale` serves three more exact40/4/4-byte requests and
reuses the actual published TLS locale in two handles. Six allocations total
144 requested/208 rounded bytes in the same64 KiB arena. Refcount reaches6;
mutex14 lives at60000090. Original stream/base flags,state,precision and tied
pointer are validated. New full bodies01688138,01685FC8,01685248,01688538
bring production to207;63 indirect sites in28 bodies. No native dispatch or HLE
service is added. All208 heap bytes and both aggregate ranges are checked.

Stop34 precedes the first24-byte facet allocation after747 entries/12,849
accesses,LR01688380,SP0FEFFA90,TOC019D3BAC,r3=24,r4=1,r5=0,r6=01CE6564.
Twelve constructors completed;13 and stream constructor remain incomplete.
Thread-ID10,locks23/22,held1 cleaned at stop. The last import NID350D454E comes
from original wrapper recursion. Older checkpoints preserve exact counts.
All791 sanitizer checks and nine compiled lifter checks pass. v0.23 passed twice
on the actual Switch, including reload, with3,353 independent checks and zero
mismatches. Post-run binary matches the sealed candidate; held native ownership
and all guest backing clean up. See the stream status audit.


## First facet construction and cleanup registration (v0.24)

`ds_entry_run_first_facet` serves the original24-byte facet and8-byte cleanup
node, snapshots84 locale bytes, constructs/restores C strings and registers
the original cleanup callback. Eight requests total176 requested/256 rounded
bytes within the same64 KiB arena. Facet600000D0 refs1;corelocale refs5.
Cache1B50430=D0,cleanuphead1CE6720=F0,atexitfree78. Six new pinned original
bodies bring production to213;indirect sites remain63 in28 bodies.

Stop35 is before virtual009E96A0 throughOPD01970348 andreturnPC01688280;
799 entries/13,238 accesses,SP0FEFFA90,TOC019B5040,r3=D0,r4=32,r5=1,
r6=7F7F7F7F7F7F7F7F. Original descriptor loading performs the TOC transition;
the target is not invoked or counted.12 constructors complete;13 and stream
construction pending. Thread-ID14,locks25/25,held0,all cleanup validated.
All17,145 sanitizer checks and nine lifter checks pass; strict Switch build and
package checks pass. v0.24 passed twice on the actual Switch, including reload:
all3,591 independent checks pass, with facet/cache/cleanup-node state and zero
guest backing at exit. Post-run binary matches the sealed candidate.
See analysis/SWITCH_FIRST_FACET_STATUS.md.

## First narrow stream (v0.25)

`ds_entry_run_first_stream` executes original009E96A0 and01684D70, completing
and registering the first narrow stream. Stop36 precedes the ninth four-byte
runtime allocation:810 entries/13,312 accesses,12 completed constructors.
The bridge retains eight accepted allocations and256 live rounded heap bytes;
all26 guest locks release normally. Production pins215 full original bodies.
All thirteen earlier metadata/runtime checkpoint APIs retain their exact stops.
See analysis/SWITCH_FIRST_STREAM_STATUS.md for validation and hardware status.

## Narrow initializer returned (v0.26)

`ds_entry_run_narrow_streams` completes01686CE8 and stops before0168BC68
with966 entries/14,378 accesses and12 completed constructors. Four streams
register; thirteen exact allocation contexts are accepted. Full registry,
0x238-byte aggregate and368 rounded heap bytes match the original-source/ELF
model in tests/audit_narrow_streams.py. Production retains215 original bodies.
All42 locks release; native/heap metadata and backing clean up on every exit.
Fourteen prior checkpoint APIs retain exact stops. See
analysis/SWITCH_NARROW_STREAMS_STATUS.md for checks and device results.

## Wide streams and constructor thirteen (v0.27)

`ds_entry_run_wide_streams` completes original0168BC68 and runtime constructor
thirteen, then stops before00745248 with1,265 entries/16,590 accesses. All
four wide streams register alongside the four narrow ones, using original
00B17510 /00010C50 /0166C0E8 conversion and TLS helpers. Source model, full
conversion oracle and67 fault fixtures pass. Eighteen CRT records require
bounded capacity32; trace capacity2048 contains the full path. Old checkpoint
boundaries remain exact. See analysis/SWITCH_WIDE_STREAMS_STATUS.md.
