# Switch clock / first-constructor checkpoint — 2026-10-23

Version 0.5 implements real monotonic microsecond time for sys_time_get_system_time
using libnx architectural counter/frequency on Switch and CLOCK_MONOTONIC on Linux.
Conversion uses a 128-bit intermediate and explicit range/frequency checks. No
synthetic clock, fixed epoch restart, per-call tick increment or wall clock.
Provider failure is an explicit stop and never returns fake successful time.

After the import saves the caller TOC, original instructions store r3 at
0x01D2B5C8 and run the initialization wrapper and static-constructor walker.
The first constructor is OPD 0x01951EA8, code 0x007CF848, TOC 0x019B5040.
Its descriptor/code/TOC and executable backing are validated before a deliberate
stop. The constructor itself is not invoked, and arbitrary indirect dispatch is
not implemented. The path has 35 entries across 19 pinned unmodified bodies,
no extra guest arenas, and preserves the previous checkpoints.

191 real-body/clock/accounting checks and 28 separate failure-fixture checks
passed under ASan, UBSan and leak detection (219 total). Tests include counter
conversion at long uptimes, overflow/frequency failures, actual native elapsed
time, timestamp memory effects and permissions, malformed OPD/TOC, executable
permission validation, repeated runs and clock-provider failure injected by a
separate host linker wrapper. Production code includes no fake clock provider.
Switch devkitA64/libnx cross-build passes -Wall -Wextra -Werror.

Artifact: switch-port/demons-souls-switch-startup.zip, v0.5.0. Device execution
is pending; the previous v0.4 result remains preserved in analysis/device-memory-v040.
No graphics, gameplay or performance result is established.

Next: audit the first static constructor and its dependent services, including
real backed allocation and synchronization before extending execution into them.

Installed v0.5 on the connected Switch through DBI MTP and verified SHA-256 by
reading the NRO back. Transfer record: analysis/device-clock-v050/transfer-verification.json.
The verified EBOOT was retained. Device execution remains pending.

## Device result

Retrieved the v0.5 log via DBI MTP. Both attempts passed: matching executable/
segment/BSS hashes, ten CRT mutex records, completed bounded memory query,
35 expected function entries and 1839 checked accesses, reached=1 stop=10,
empty error. Clock frequency=19200000 Hz; both raw tick conversions exactly
match logged microseconds and returned r3. Time advanced across reload. The
accepted checkpoint also verifies the actual stored guest timestamp.

The first constructor resolves to descriptor 0x01951EA8, target 0x007CF848,
TOC 0x019B5040, executed=0 in both attempts. Final cleanup reports backed bytes=0.
Raw evidence: analysis/device-clock-v050/loader.log. Structured verification:
analysis/device-clock-v050/device-result.json. This supersedes the earlier
pending-device status; v0.5 execution and reload passed on this Switch.
