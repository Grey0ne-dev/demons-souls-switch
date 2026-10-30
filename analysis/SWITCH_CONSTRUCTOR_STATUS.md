# First constructor prefix / native mutex checkpoint — 2026-10-23

Version 0.6 executes a bounded prefix of first static constructor 0x007CF848.
It does not claim that the constructor completes. The path reaches the next
singleton initializer 0x00AE7768, which is an explicit pre-execution boundary.
There are 53 function entries across 36 pinned unmodified bodies.

Implemented required imports: main guest thread ID query, actual native
uncontended mutex lock and unlock. Constructor mode backs eleven guest mutex
records using a 16-record libnx Mutex pool (pthread mutexes on Linux). Native
recursion, contention, timed waiting, scheduling and cross-thread ownership
transfer remain unsupported and stop explicitly. All native records are torn
down after the checkpoint or a fault, including held locks.

Actual guest atexit code registers destructor OPD 0x01967920 at 0x01CE58E8,
changes free slots from 80 to 79, and uses CRT mutex 0x01CE5940. The constructor
creates object mutex 0x01B92D00. No new guest arenas are allocated. Two audited
indirect calls validate OPD/code/TOC/executable backing and restore bctrl LR
values omitted by the lift (0x016B84F4 and 0x00A667D4).

A constructor attempt requires a fresh image, preventing partially initialized
guest state from manufacturing a repeat success. The loader's A button already
destroys and reloads the image. No checkpoint execution is resumable.

Validation: 215 real-body/backend checks plus 40 separate service/fault-fixture
checks (255 total), ASan/UBSan/leak detection. Fault fixtures include an invalid
wait timeout and a guest fault immediately after a real native mutex acquisition;
all native state is cleaned in both cases. Strict devkitA64/libnx cross-build
passes. A C-linkage correction for the libnx mutex declarations was verified by
the final Switch link. Version 0.6 device execution remains pending.

Next: singleton initializer 0x00AE7768 and its allocation/dispatch dependencies.
Graphics, gameplay and performance remain unimplemented/unverified.

Installed v0.6 through DBI MTP and verified its SHA-256 by reading it back from
the SD card. Transfer evidence: analysis/device-constructor-v060/transfer-verification.json.
Existing verified EBOOT retained. Device execution is still pending.

## Device result

Retrieved v0.6 log via DBI MTP. Both attempts passed: matching file/segment/BSS
hashes, 53 expected entries and 2022 checked accesses, constructor started=1
completed=0, one thread-ID query, one native lock/unlock on 0x01CE5940, eleven
mutex records including object mutex 0x01B92D00. Actual atexit state reports
79 free slots and callback 0x01967920. No native locks were held at the boundary
and no native records survived teardown. Both reach stop=12 at 0x00AE7768 with
LR 0x007CEF28, SP 0x0FEFFCA0 and empty error. Clock conversion matches raw ticks
and time advanced across reload. Final guest backing cleanup reports zero.

Raw evidence: analysis/device-constructor-v060/loader.log. Structured validation:
analysis/device-constructor-v060/device-result.json. This supersedes the earlier
pending status. The first constructor remains incomplete; singleton initialization
and later gameplay are not established.
