# Switch memory strategy: next experiment

Research checked against primary libnx, nx-hbloader and Atmosphère source on 2026-10-22. This is a design and experiment plan. No Switch device has been tested, and the findings do not establish that the game will fit or run.

## Answer to the flat-address-space question

`svcMapMemory` and `svcMapPhysicalMemory` do **not** supply Linux-style anonymous `mmap(MAP_NORESERVE)` with automatic zero-page backing on access. A large **unmapped virtual interval** can be tracked in userspace without backing the whole interval, but each accessible subrange needs explicit physical backing. Whether an appropriately aligned interval exists, the mapping SVC is permitted, and enough memory-management resources remain must be measured in the actual launch process.

| Mechanism | What its implementation does | Implication for this runtime |
| --- | --- | --- |
| `svcMapMemory(dst, src, size)` | Maps already backed, eligible RW source pages to a stack-compatible destination. The source becomes inaccessible/borrowed. | Useful for particular alias/stack operations; it neither creates a 4 GB unbacked reservation nor allocates fresh guest pages. [libnx SVC API](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/kernel/svc.h), [Atmosphère MapMemory checks](https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libmesosphere/source/svc/kern_svc_memory.cpp) |
| `svcMapPhysicalMemory(addr, size)` | Explicitly allocates and maps missing physical pages. Atmosphère checks alignment, a nonzero process system-resource budget and alias-region containment, then charges the process physical-memory resource limit. | Could back selected guest subranges if the target process permits it. Mapping 4 GB would request real backing, so do not attempt it. [SVC validation](https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libmesosphere/source/svc/kern_svc_physical_memory.cpp), [page allocation implementation](https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libmesosphere/source/kern_k_page_table_base.cpp) |
| `virtmemAddReservation(ptr, size)` | Adds an interval to libnx's linked list of reservations while its mutex is held; it performs no kernel mapping. | Suitable for coordinating an unbacked interval among participating libnx users. It does not enforce ownership against direct SVC users, validate the supplied address, or make holes readable. [reservation API](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/kernel/virtmem.h), [implementation](https://github.com/switchbrew/libnx/blob/master/nx/source/kernel/virtmem.c) |
| `virtmemFindAslr(size, guard)` | Searches free general address space, explicitly excluding the alias and heap regions. | Its result is unsuitable as a `svcMapPhysicalMemory` destination. Inspect the process's actual alias region instead. [address search](https://github.com/switchbrew/libnx/blob/master/nx/source/kernel/virtmem.c) |

The current PC boot harness uses a 4 GB guest window plus 64 KB guard, on a **4 GB-aligned host base**. Several host-to-guest pointer conversions depend on truncating pointers to their low 32 bits (`runtime/memory/vm.h`, `runtime/ppu/ppu_loader.cpp`). Merely finding a 4 GB virtual gap with a different alignment does not preserve this invariant.

## Two additional constraints

**Physical headroom must include the host heap.** nx-hbloader allocates its heap from almost all available process memory and passes a heap override to homebrew. Libnx normally uses that override; without one, its default initialization also sizes its heap from available memory. Consequently a tiny physical mapping can fail even when a large virtual interval exists. Log the heap override and budgets; do not shrink a live loader heap to force the experiment through. A production design would need deliberate allocation of the host/loader heap versus guest pages before either allocator uses those regions. [nx-hbloader heap and configuration](https://github.com/switchbrew/nx-hbloader/blob/master/source/main.c), [libnx heap initialization](https://github.com/switchbrew/libnx/blob/master/nx/source/runtime/init.c)

**The standard libnx exception hook does not retry a fault.** Its exception entry redirects execution to `__libnx_exception_handler`; if that handler returns, the stock assembly calls `svcBreak`. Installing that hook is therefore insufficient to reproduce the PC runtime's commit-on-fault behavior. A resumable exception implementation would require separate design and validation, including concurrency and fault-state handling. Prefer explicit commits in the first port slice. [libnx exception assembly](https://github.com/switchbrew/libnx/blob/master/nx/source/runtime/exception.s)

## Safe device diagnostic

The separate `switch-probe` work should report these measurements, clearly labeled as a memory diagnostic rather than a game build:

1. Record launch type, Horizon/Atmosphère identification when available, heap override address/size, and SVC availability hints. Query `InfoType_AliasRegionAddress/Size`, `AliasRegionExtraSize` when available, `AslrRegionAddress/Size`, `HeapRegionAddress/Size`, `StackRegionAddress/Size`, `TotalMemorySize/UsedMemorySize`, and `SystemResourceSizeTotal/Used`. Record each query's `Result`; unsupported queries are useful evidence. These names are public libnx info types. [libnx SVC declarations](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/kernel/svc.h), [environment utilities](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/runtime/env.h)
2. Under `virtmemLock`, inspect the alias interval using `svcQueryMemory`. Search for an entirely unmapped **4 GB-aligned** slice covering `0x100010000` bytes, accounting for any alias extra area. Register only a userspace reservation and re-query usage. Do not touch its holes. A reservation adds bookkeeping, not 4 GB of physical pages. If no such slice exists, record the layout and stop this strategy's probe.
3. If the SVC hints, system resources and remaining memory permit it, explicitly map a tiny owned subrange, for example 64 KB at guest offset `0x10000`. Record the SVC result and query its actual mapped state before any access. Only after success, write/read the owned mapped range through `base + guest_offset`, then unmap it and release the reservation. On mapping failure, record the error without reading or writing that range. No 4 GB physical mapping, heap resizing or deliberate unmapped access belongs in this diagnostic.

A failure caused by exhausted resource limits or launch permissions is **inconclusive about other launch configurations**. Success shows only that this mapping primitive and alignment work in that process; it does not establish game memory capacity or a renderer.

## First real game port slice after the PC baseline

The first useful game milestone is **loading the actual decrypted EBOOT into a Switch guest-memory backend and validating its segments and entry descriptor**, with a bounded physical allocation budget. This uses real runtime/game inputs and tests the memory design that the next stage needs.

Implement reservation, explicit commit, protection and release operations behind a Horizon memory backend. Keep `host = vm_base + guest` only if the diagnostic confirms the alignment and interval requirements. Commit ELF `PT_LOAD`/BSS ranges before the existing loader copies bytes, then verify segment checksums and the entry OPD/TOC against the Linux baseline. Add stack/TLS and guest allocator commits before executing actual game entry. `sys_memory_allocate` already calls `vm_commit`; `port/des_heap.cpp` currently returns ranges without doing so, and loader/TLS copies also assume backing already exists. Those paths need explicit coverage.

The acceptance artifact is a device log showing the real EBOOT identity, matching loaded segment/BSS checks, valid entry/TOC translation, allocated byte count and successful cleanup. Execution can advance only after each required allocator/thread/HLE operation is implemented; reaching a selected real guest initialization checkpoint is a later milestone.

If the aligned sparse window cannot be supported, the alternative is segmented guest translation using ordinary backed arenas. That requires auditing direct `vm_base + address` accesses, HLE pointer lifetimes, DMA copies and host-pointer truncation throughout the runtime. It is a larger architectural change and should be decided from the diagnostic results.

This research changed documentation only. It installed no dependencies and made no runtime source changes.
