# Singleton / reader-writer creation checkpoint — 2026-10-23

Version 0.7 enters the singleton/shared-manager constructors and implements
sys_rwlock_create (LV2 120), backed by a real libnx RwLock and a guest handle
registry (pthread_rwlock on Linux). The observed private, priority-protocol
creation is supported. Reader/writer acquisition, waiter scheduling, IPC and
guest threads are not implemented.

The original path has 68 entries across 49 pinned unmodified bodies. Singleton
0x01B4C6E8 enters manager creation at 0x01B91128. Original constructors install
the manager self pointer, derived reader/writer vtable 0x01872060 at 0x01B911A8
and an uninitialized -1 handle at +4. The service checks output and all 32
attribute bytes, creates a native object, writes guest handle 1 at 0x01B911AC
and sets r3=0 before a deliberate stop. Neither singleton nor first constructor
is complete; caller continuation has not executed after that service.

Four audited indirect targets now cover the constructor and three virtual
methods; manager lock/state-query returns are 0x00A35A18 / 0x00AB63B8. Known
descriptors, code, TOC, executable backing and the manager lock call context
are checked. Unknown dispatch remains a failure.

Native state at stop: eleven mutex records, two locks, one unlock, one held
manager mutex, and one new reader/writer object. Teardown releases/destroys
all native state, including held-lock paths. Guest backing is unchanged.
Attempts require a fresh image; the loader A button reloads it.

246 real-body/backend checks and 68 separate fault-fixture checks passed
(314 total), ASan/UBSan/leak detection. Tests include actual manager fields and
native handle output, pool limits, invalid output/attribute pointers, unsupported
IPC attributes, native creation failure and indirect target/TOC faults.
Switch devkitA64/libnx strict build passes. Device execution is pending.

ABI references are linked in switch-port/entry/README.md: RPCS3's sys_rwlock
attribute structure, constants and creation implementation. The syscall number
comes from the pinned local LV2 table. In particular this is a reader/writer
lock request, not sys_mutex_create (which the table numbers 100).

Next: continue the manager after this return and audit its remaining initialization,
reader/writer operations, allocation and dispatch dependencies. Graphics/gameplay
are not implemented or established.

Installed v0.7 through DBI MTP and verified the NRO SHA-256 by reading it back.
Transfer evidence: analysis/device-singleton-v070/transfer-verification.json.
The existing verified EBOOT is retained. Device execution is still pending.

## Device result

Retrieved v0.7 log via DBI MTP. Both attempts passed: file/segment/BSS hashes
match, all 68 expected entries and 2167 checked accesses, singleton/manager
addresses match, syscall 120 creates registered reader/writer handle 1 and
returns CELL_OK. Attributes report protocol=2, pshared=0x200. The first
constructor remains started=1 completed=0; original manager continuation after
the syscall has not executed. Both runs reach stop=14 with empty error,
LR 0x00AB63B8 and SP 0x0FEFFA00.

The manager mutex is intentionally held at the service stop (two locks, one
unlock); teardown releases it. Native mutex and reader/writer live counts
after cleanup are both zero. Final guest backing is zero. Clock conversion
matches raw hardware ticks and time advances across reload.

Raw log: analysis/device-singleton-v070/loader.log. Structured validation:
analysis/device-singleton-v070/device-result.json. This supersedes the earlier
pending status: v0.7 execution and reload passed on the user's Switch.
