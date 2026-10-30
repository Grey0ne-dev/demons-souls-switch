# First name-registry insertion — 2026-10-24

Version 0.10 continues after completed singleton startup into 0x00AE4D38.
The actual lazy registry constructor creates container 0x01B92A78 and uses
manager interface 0x01B91190. Original allocation code obtains a second
0x2000-byte allocation from the existing 384 KiB pool, returning 0x01AD6D90.
Original code inserts one 16-byte record for DsMenuDlgBloodMsg:

- singleton: 0x01B4C6E8
- name pointer: 0x016F8410
- empty-string pointer: 0x016F8320
- name length: 17, computed by the original checked strlen body

Container begin/end/capacity are 0x01AD6D90, 0x01AD6DA0 and 0x01AD8D90.
The first 384-byte allocation at 0x01AD6C00 and its range record are preserved.
The new free chunk begins at 0x01AD8D90. No extra guest backing arena is added.

The v0.10 trace reaches 115 entries across 74 pinned complete bodies and
2,756 checked accesses. Native lightweight mutex calls balance 4/4, native
writer calls 1/1. There is one existing rwlock and the original initial CAS;
no new reader/writer service or CAS is required by this insertion. Native
resources are cleaned after every bounded stop. Restart requires a fresh
image; the loader A button performs that reload.

The deliberate stop is before callback-registry initializer 0x00AE4490,
stop=18, LR 0x00AE50E0, SP 0x0FEFFC30, TOC 0x019B5040, r3=0x01AD6DA0.
The singleton is complete, but the first outer constructor remains incomplete.
Callback-registry construction, callback registration and sorting have not
executed at this checkpoint.

Nine added pinned bodies are AE4D38, AE2148, ADEBA0, 7CEC40, 7CEC48,
00010BA0, 016693E0, B27658 and ADCAB0. The full merged 00010BA0 body is kept;
only its first trampoline to strlen executes here. Unknown and alternate/error
paths remain strict stops. Audited callsite LRs and body hashes are recorded
in generator provenance. The older pre-AE4D38 checkpoint remains available.

Validation status: Switch cross-build and all 479 host sanitizer checks passed:
341 main, 93 service/fault-fixture, 30 atomic-backend and 15 writer-backend checks.
The installed NRO was read back over USB and matched the 334,021-byte build,
SHA-256 `3b632e6b42aa431823b8b989251871f6b73bf727631a2c1ab981465a09512141`.
The installation record is analysis/device-registry-v0100/transfer-verification.json.
Version 0.10 passed two attempts on the user's Switch, including a fresh reload.
Independent validation passed 583 checks with zero mismatches: original ELF
and copied segment/BSS hashes, all 115 ordered entries (95 from v0.9 plus the
20-entry registry extension), the first 32 access records, exact stop ABI,
registry/allocation diagnostics, memory query, services and cleanup. Both
attempts report 2,756 checked accesses, mutex calls 4/4, writer calls 1/1 and
zero held/live native resources. Final exit reports zero backed guest bytes.
Both 19.2 MHz tick conversions are exact; the reload advances by 71,459,410
ticks and 3,721,844 microseconds. The source log SHA-256 is
`fdefa55f317fec5e588dc737818ed4ab07a351d62028efdd3c6898b13f537a41`.
Evidence and provenance are recorded in analysis/device-registry-v0100/loader.log
and device-result.json; transfer-verification.json now records the hardware pass.
The log exposes checkpoint diagnostics and a 32-access preview; the checkpoint
validator enforces tuple and allocator-link invariants without dumping those
guest words separately. Version 0.9's earlier single device attempt remains
preserved in analysis/device-startup-v090.

Next: audit and implement the separate callback registry, its callback/object
pairs and sorting adapters before claiming AE4D38 returns. Graphics, gameplay
and performance remain unimplemented/unverified.
