# Switch memory-query / CRT-table checkpoint — 2026-10-23

Version 0.4 implements LV2 syscall 352 using conservative configured guest-memory
accounting, not the desktop's hard-coded 213 MiB or the console process allowance.
The service writes total/available 32-bit big-endian words after validating the
full output span, and returns CELL_OK. All existing backed arenas are charged
and available bytes round down to 1 MiB. Default: 48 MiB total / 17 MiB available.
It does not allocate backing, implement memory allocation, or guarantee capacity
for later host allocations.

The normal original heap-init branch then returns; newly extracted original
functions run the next CRT initializers, ten distinct guest mutex record
creations, stream metadata and descriptor tables. The exact path is 31 entries
across 15 pinned bodies. It stops before sys_time_get_system_time, NID 0x8461E528,
at LR 0x12708 with restored main TOC/stack/TLS. The clock remains unimplemented.
Lock/wait operations and secondary threads remain unsupported.

Host tests cover actual syscall output, budgets 48/64 MiB, independent extra
backing charges, exhausted/sub-page/oversized/invalid accounting, all ten mutex
records, stream globals, standard descriptors and all 253 remaining descriptor
slots, repeat runs, unknown syscalls and full-span output validation. A test-only
heap threshold exercises the low-memory warning branch, stopping at unsupported
syscall 403. Null and partially backed output fixtures remain separate from
production source generation.

The Switch cross-build passes with strict warnings. Extended host sanitizer
validation and device-transfer results are recorded separately as completed.
Version 0.3's two successful device runs remain preserved under
analysis/device-crt-v030. No v0.4 device execution or gameplay is claimed here.

Next: a monotonic clock service and continued startup audit; explicit backed
allocation under the same budget is required before serving allocation requests.

Final host validation: 153 checks on original bodies/accounting plus 23 checks
across separate unknown-import, unknown-syscall, null-output and crossing-output
fixtures passed (176 total), with ASan, UBSan and leak detection.

Installed the v0.4 NRO through the active DBI MTP connection and verified it by
reading it back and comparing SHA-256 against the tested build. Transfer record:
analysis/device-memory-v040/transfer-verification.json. Existing verified EBOOT
was retained. Device execution of v0.4 is still pending.

## Device result

Retrieved the v0.4 log through DBI MTP. Both attempts passed: file/segment/BSS
hashes match, memory query completed once with total=50331648 and available=17825792
after charging 31621920 bytes, ten expected mutex records, 31 expected function
entries, 1816 checked accesses, reached=1 stop=9 at NID 0x8461E528, empty error.
The clock service did not execute. Final cleanup reports backed bytes=0.

Raw log: analysis/device-memory-v040/loader.log. Structured validation:
analysis/device-memory-v040/device-result.json. This supersedes the earlier
pending-device status: v0.4 execution and reload are now verified on this Switch.
No graphics/gameplay result is established.
