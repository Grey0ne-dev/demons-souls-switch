# Pool allocation and singleton checkpoint — 2026-10-24

Version 0.9 initializes the existing 384 KiB static pool at 0x01AD6B78,
performs its first aligned 384-byte allocation at 0x01AD6C00, registers the
range under a real native writer lock, and completes/publishes singleton
0x01B4C6E8. The deliberate stop is before 0x00AE4D38, LR 0x007CF75C,
SP 0x0FEFFD40, r3=singleton. The first outer constructor remains incomplete.
Graphics and gameplay are unimplemented.

65 pinned lifted bodies, 95 expected entries. The atomic helper uses a real
endian-correct checked native CAS for the single guest thread. Native mutex
locks/unlocks balance 3/3, writer locks/unlocks 1/1. Twelve mutex records and
one rwlock object are destroyed on stop. Guest backing remains 31,621,920 bytes
until image destruction; allocation uses existing pool bytes without adding
arenas. The upstream NULL-pool patch in 0x00AC8078 is explicitly documented,
unsupported, and never executed on the accepted path.

424 host checks passed under ASan, UBSan and leak detection: 291 main checks,
88 service/fault-fixture checks, 30 atomic-backend checks and 15 writer-backend
checks. These include exact independent allocator/free-list/range/singleton
invariants, fresh reload, rejection of in-place restart, held-writer cleanup,
unsupported writer wait, forced fallback refusal and atomic-provider failure.
The native backends and indirect-return addresses were independently reviewed.
Runtime acceptance validates final allocator/singleton fields, and the range
method also checks its exact incoming LR. A corrupt descriptor TOC fails
explicitly with full cleanup. The devkitA64/libnx cross-build passes with
warnings treated as errors; generated upstream formatting/unused warnings
are suppressed without rewriting pinned bodies.

Version 0.9 passed on the user's Switch. The next code audit starts at initializer
0x00AE4D38; no gameplay or performance result is implied by this startup work.

Installed through DBI MTP and verified byte-for-byte by USB readback. NRO:
317,637 bytes; SHA-256
`a236b83f98dee44bed4c659acc3187c31d52f595279c6ea8a5709d45f1be248f`.
Transfer record: `analysis/device-startup-v090/transfer-verification.json`.
The existing verified EBOOT was retained. The staged ZIP passed CRC and
per-file SHA-256 verification. MTP was released after transfer and log collection.

## Device result

Retrieved the v0.9 log through DBI MTP. One successful attempt: all executable,
segment and BSS hashes match; the exact 95-entry trace and 2,505 checked accesses
reach stop 16 at 0x00AE4D38 with empty error and the expected LR/SP/r3 ABI.
Pool setup ran, the first 384-byte allocation returned 0x01AD6C00, CAS succeeded
1/1 and the bootstrap fallback count is zero. The singleton completed; the first
outer constructor remains incomplete.

Native mutex locks/unlocks balance 3/3, writer locks/unlocks 1/1. No locks were
held at the stop; both native registries are empty after teardown. Final guest
backing cleanup is zero. The hardware counter reports 19.2 MHz and its logged
microseconds equal the checked tick conversion. An independent review also
matched every expected checkpoint field and the full function trace.

Raw log: `analysis/device-startup-v090/loader.log`. Structured validation:
`analysis/device-startup-v090/device-result.json`. Only one attempt is present,
so this log establishes no hardware reload result for v0.9. Next-step static
findings are in `analysis/STARTUP_AFTER_SINGLETON_AUDIT.md`.
