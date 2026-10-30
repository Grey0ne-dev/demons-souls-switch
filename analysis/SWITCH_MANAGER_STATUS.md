# Manager publication / static-pool handoff — 2026-10-23

Version 0.8 continues after sys_rwlock_create. Original guest code consumes
handle 1, completes manager-object construction, publishes 0x01B91128 at
0x01B9111C, and releases the held startup mutex through an audited virtual
unlock. Native lock/unlock counts are now balanced (2/2). It validates the
configuration getter's complete 384 KiB pool interval at 0x01AD6B78 against
existing readable/writable ELF backing. No extra guest arena is allocated.

The deliberate stop is before allocator initializer 0x00B25B58, LR 0x00A3585C,
SP 0x0FEFFAB0, r3=manager, r4=pool base, r5=0x60000. The singleton and first
constructor remain incomplete. Allocator initialization and allocation requests
have not executed at this boundary. Next work includes the pool initialization
helpers and their checked atomic/store-conditional operation.

52 pinned original bodies, 72 entries. Added bodies: 0x00967998, 0x00A662C8,
0x0002F0B8. The new indirect unlock validates descriptor/code/TOC/executable
backing and call context, restoring bctrl LR 0x00A35A48. Earlier checkpoints
remain available. All native resources are destroyed on stop; attempts need a
fresh image, as provided by the loader A button.

270 real-body/backend checks and 68 fault-fixture checks passed (338 total),
ASan/UBSan/leak detection; strict devkitA64/libnx cross-build passes. Tests
cover real publication/control words, configuration, complete pool backing,
balanced native ownership and failure cleanup with corrupt unlock TOC or an
RX pool pointer. v0.8 device execution remains pending.

Installed v0.8 through DBI MTP and verified the NRO by reading it back and
comparing SHA-256. Record: analysis/device-manager-v080/transfer-verification.json.
Existing verified EBOOT retained. Device execution remains pending.

## Device result

Retrieved v0.8 log via DBI MTP. One successful attempt: all file/segment/BSS
hashes match, 72 expected entries and 2208 checked accesses, manager published,
pool 0x01AD6B78 of 393216 bytes validated. Actual native mutex counts balance
(2 locks / 2 unlocks), no held locks at stop, and native mutex/rwlock counts
after teardown are zero. Reached=1 stop=15, empty error, expected pool arguments,
LR 0x00A3585C and SP 0x0FEFFAB0. Final guest backing cleanup is zero.
Allocator setup did not execute; the outer constructor remains incomplete.

Raw log: analysis/device-manager-v080/loader.log. Structured validation:
analysis/device-manager-v080/device-result.json. This supersedes the earlier
pending-device status: the v0.8 checkpoint passed on this Switch. This log
contains one attempt; it does not establish a reload result for this version.
