# Startup after singleton: read-only audit

Audited the pinned BLUS30443 1.00 ELF (`90eaa5e9e48b4874292badc311ccea253e6a622ff188bfbdbe76e6f6821269d8`) and local PPU lift. No runtime or generator changes were made. Addresses and expected state below are a static audit, not a new execution result.

## Recommended next checkpoint

Continue through `00AE4D38`'s first name-registry insertion, then stop **before `00AE4490`** (direct call at `00AE50DC`, return LR `00AE50E0`). This adds a real static registry, one additional guest allocation of `0x2000`, and a registered 16-byte record for `DsMenuDlgBloodMsg`. It avoids mixing in the second registry and callback sorting. The outer constructor remains incomplete.

At this boundary expect SP `0x0FEFFC30`, TOC `0x019B5040`, LR `0x00AE50E0`. This is the existing pre-`AE4D38` SP `0x0FEFFD40` minus its `0x110` frame. Do not treat r3 as a new callee argument: `AE4490` does not consume its incoming r3.

## Immediate accepted-path dependencies

1. `AE4D38` receives singleton `0x01B4C6E8`; calls `AE2148`, LR `AE4D94`.
2. `AE2148` lazily publishes raw static pointer `0x01B92A75` in slot `0x01B92A70`, aligns it to container `0x01B92A78`, and initializes `{allocator=0x01B91190, begin=0, end=0, capacity=0}`. It sets flag byte `0x01B92A88=1` and flag `0x01B925B0=1`. Its `B0AF28` static synchronization singleton is already initialized by v0.9. Shared manager-cache slot `0x01B91BEC` was populated by `AE7768`, so `A35DB0` is skipped on this continuation.
3. `AE2148` obtains manager status through interface slot `+0x10`, OPD `0x01968548` -> existing `B25E58`, then existing `A34BB8`/`A34C48`. Descriptor is held in **r9** here; on this fresh-container/cached-manager path incoming lifted LR is **`0x00AE227C`** (last call to `B0AF28`) and original bctrl return LR is **`0x00AE21DC`**. This differs from `AE7768`'s r11/`AE7854` -> `AE77E8` callsite and requires a distinct audited indirect adapter case.
4. `AE4D38` calls `ADEBA0`, LR `AE4DB4`, output at frame+`0x90` (`0x0FEFFCC0`). It invokes singleton vtable `0x0186A628` slots `+8`/`+C`: OPDs `0x01951D80` -> `7CEC40` and `0x01951D88` -> `7CEC48`. Both are leaf field getters. Their bctrl return LRs are **`ADEBF8`** and **`ADEC20`**. They return name pointer `0x016F8410` and empty string pointer `0x016F8320`.
5. `ADEBA0` calls `00010BA0` at LR `ADEC30`; its first reachable action is the import trampoline to translated **`016693E0` (`strlen`)**. No new HLE service is needed for this operation. It returns length **17**. Record becomes `{0x01B4C6E8, 0x016F8410, 0x016F8320, 17}`.
6. Empty registry takes `AE4DEC`, requests allocation size `0x2000`, alignment argument4. Interface `0x01B91190`, vtable slot `+0x2C`, OPD **`0x01968620` -> `B27658`**, bctrl return **`AE4E18`**. `B27658` is an interface allocation wrapper using manager at interface+4 (`0x01B91128`), not a call to `B26C18`.
7. `B27658` locks existing manager mutex through OPD `0x019629E8` -> `A664E0` (descriptor in r11, bctrl return **`B276AC`**), calls existing `AC8578` -> `AC8078` (direct-call LR `B276C8`), then unlocks through OPD `0x019629D8` -> `A662C8` (descriptor in r10, bctrl return **`B27708`**). Reuse native uncontended lwmutex services. Current allocation-specific adapters and counters cover `B26C18` only and must be generalized deliberately.
8. After allocation, no old buffer exists to copy or free. The binary-search loop has count0. `ADCAB0` copies the record to stack temporary frame+`0x80`, then to registry payload. The reserved 512-record capacity avoids the reallocation branches. Execution reaches `AE50DC`.

All OPDs above use main TOC `0x019B5040`. Restore each omitted bctrl LR using the original ELF instruction address, and validate the appropriate descriptor register, target, TOC and executable backing.

## Expected state for independent checks

Given the accepted v0.9 allocator state (free chunk starts `0x01AD6D80`), the next allocation should split that chunk and return **`P=0x01AD6D90`**, 16-byte aligned. This exact address is an allocator inference to confirm in execution; record/container invariants do not require hardcoding P.

| Address | Expected after first registry insertion |
| --- | --- |
| `0x01B92A70` | raw static pointer `0x01B92A75` |
| `0x01B92A78` | manager interface `0x01B91190` |
| `0x01B92A7C` | P |
| `0x01B92A80` | P + `0x10` |
| `0x01B92A84` | P + `0x2000` |
| P + `0x00` | singleton `0x01B4C6E8` |
| P + `0x04` | name `0x016F8410` |
| P + `0x08` | empty string `0x016F8320` |
| P + `0x0C` | 17 |

The new free chunk should start at P+`0x2000` (`0x01AD8D90`). Validate its allocator links/accounting independently rather than inferring success solely from the returned pointer. Expected additional native lwmutex lock/unlock pair: one. No additional rwlock creation/write acquisition, LV2 syscall, atomic CAS or guest backing arena is apparent before this checkpoint. Existing pool backing serves the allocation.

## What follows / risks

- `AE4490` lazily initializes a separate 8-byte callback-entry registry (slot `0x01B92A54`). `AE4D38` then reserves `0x1000`, calls singleton slots `+0x10/+0x14/+0x18/+0x1C`, appends four callback/object pairs through `AE47A0`, and sorts through `AE0BE8`. Those larger bodies and precise normal branches need another audit before claiming `AE4D38` returns. Its full direct target set also includes `ADCAD8`, `AE1E40`, assertion `A9DD58`, and failure unwinder `01674A5C`; several are alternate/error paths only.
- `00010BA0` is a merged import-stub body (~102 KiB of generated C++). Only its **first** trampoline to `016693E0` is reachable from this entry. A textual reference scan reports hundreds of later, unreachable trampoline targets; do not describe those as the immediate executed call graph. Pin the full existing body if retaining the generator's unmodified-body policy.
- Allocation failure paths inspect TLS at TP-28104 and may call `01674A5C` with TP-28648. That unwinder restores guest registers and uses direct `vm_base` VMX accesses. It is incompatible with the checked segmented-memory adapter and must remain a strict unsupported stop, along with assertions, rather than being imported casually.
- The inherited `AC8078` NULL-pool `ds_malloc` patch remains irrelevant on this normal path and must continue to stop if reached. No general allocator fallback is authorized by this audit.
- Keep native held-lock fault teardown tests for the new interface allocation wrapper. Do not advertise full concurrent PPC reservation semantics: no new atomics are reached here, and the current implementation remains a bounded single-host-thread adapter.

## Canonical pinned body hashes

Hashes use the generator's exact `^void func_...` through column-zero closing brace regex, without trailing blank lines.

| Function | Chunk | SHA256 |
| --- | --- | --- |
| `AE4D38` | 005 | `602707cff2997cc1fcf377c35a945d41f7cef98dca75023aec4f1095d110bb0b` |
| `AE2148` | 005 | `47078ce99efc2b204bd8fbed07114a67feedf270d3262d32ce982b85a395f3c9` |
| `ADEBA0` | 005 | `84087ccff2f8ece43c7027e9f3bf18f6b87d440a7a77f44c9adfafe9a32be121` |
| `7CEC40` | 003 | `15d7d3c2795364bfb58f5cc7ac6ee01edec8acdd3891901a661d5b8a8a884763` |
| `7CEC48` | 003 | `6706b6f126ec80d020c49d2de4398c224ddb272fc634a61df8a94e77daf7ec8d` |
| `00010BA0` | 012 | `b834619481cdb0b4b1f374fc01f88ece0dc46178fd97056e8976a76259c00356` |
| `016693E0` | 011 | `97e6397393f99595b825be1417e30ef8083eb33513a8bd7b10b0fa9d0a5ec850` |
| `B27658` | 005 | `cc30cfd8f66733ccef88d35e5ab9ce111dcee336a14bb65d65155d6fec216253` |
| `ADCAB0` | 005 | `7053758a6d8215c531b5daa796c03afc41cf0eed5229b17996a719009c675c4d` |
