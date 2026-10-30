# Switch CRT checkpoint — 2026-10-23

User-supplied v0.2 device log confirms two successful real image loads, matching
file/segment/BSS hashes, three lifted function entries, initialized TLS with
r13=0x0E007000 and CELL_OK, and cleanup with backed bytes=0. The v0.1 text in
that log was a stale heading and is fixed in v0.3.

Version 0.3 extends execution past TLS. Import TOC save slots now match the
pinned runtime dispatcher; argv/envp are materialized in a separate writable
arena rather than desktop RX scratch or null envp. Four more pinned original
translated functions run through heap initialization and sys_lwmutex_create.
A guest mutex record is initialized; lock/wait services remain unsupported.
The checkpoint deliberately stops before syscall 352, without fabricating a
memory-size response. The trampoline's erased function-pointer invocation was
also corrected after UBSan caught it on the newly traversed route.

Validation: 117 real-body tests plus 6 unknown-import and 5 unknown-syscall
fixture tests passed with ASan, UBSan and leak detection. Switch devkitA64/libnx
cross-build passed with -Wall -Wextra -Werror. Production generated bodies remain
unmodified, with pinned hashes and updated provenance; test mutations stay separate.

Current artifact: switch-port/demons-souls-switch-startup.zip, version 0.3.0.
Device validation of this version remains pending. No graphics or gameplay result.

Next work: bounded guest memory accounting and sys_memory_get_user_memory_size,
then continue auditing heap initializer branches before including additional code.

Installed v0.3 NRO on the connected Switch through DBI MTP and verified its
SHA-256 by reading it back. The previous TLS device log was preserved in
analysis/device-crt-v030/prior-tls-device.log. The verified EBOOT was retained.
USB transfer is not evidence of executing this new version.

## Device result

Retrieved analysis/device-crt-v030/loader.log through DBI MTP. Both v0.3 attempts
passed the exact checkpoint: matched image/segments/BSS, TLS initialized, CRT
mutex at 0x01CE6450, argv/envp at 0x10000000/0x10000010, library TOC 0x019D3BAC,
seven functions and 1,131 checked accesses, reached=1 stop=8, no error. Both
stopped before syscall 352. Final cleanup reports backed bytes=0. Structured
validation is in analysis/device-crt-v030/device-result.json. This establishes
v0.3 execution and reload on the user's Switch; no graphics/gameplay result.
