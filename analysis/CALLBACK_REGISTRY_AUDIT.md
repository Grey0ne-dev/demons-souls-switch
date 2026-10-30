# Callback-key registry and AE4D38 return audit

Static audit of the pinned BLUS30443 1.00 ELF and local lift. No runtime/generator changes or execution claims. This continues `STARTUP_AFTER_SINGLETON_AUDIT.md`, whose proposed checkpoint has 115 traced function entries before `AE4490`.

## Recommended bounded milestone

Complete the second registry's initialization, reserve `0x1000`, append and sort four key/object pairs, and allow **`AE4D38` to return**. Then execute the already-supported `7CEE78` singleton lookup and stop before the next singleton initializer **`782468`**, direct-call return LR **`0x007CF770`**. Expected boundary: **137 function entries**, SP **`0x0FEFFD40`**, r3 **`0x01B4C6E8`**, TOC **`0x019B5040`**. Outer static constructor `7CF5D0` is still incomplete.

This stage introduces seven complete lifted bodies: `AE4490`, `7CEE00`, `7CEE18`, `7CEE30`, `7CEE48`, `AE47A0`, `AE0BE8`. No new native service or atomic operation is apparent on the fresh accepted path. It needs one more existing-pool allocation and one manager mutex lock/unlock pair.

## Container and reserve path

- `AE4490` reads static slot `0x01B92A54`; initially zero. Existing `B0AF28` returns its initialized synchronization object with no extra constructor or native creation. `AE4490` publishes **raw pointer `0x01B92A59`**, returns aligned container **C=`0x01B92A5C`**, writes its fields `{allocator=0x01B91190, begin=0, end=0, capacity=0}`, and sets flag byte `0x01B92A6C=1` / initialization flag `0x01B925B8=1`.
- Shared manager-cache slot `0x01B91BEC` is already populated, so `A35DB0` is skipped. Manager status uses OPD `0x01968548` -> `B25E58`, descriptor **r9**, pre-LR **`AE45C4`** -> original bctrl return **`AE4524`**. Arguments r3=`0x0FEFFBF0`, r4=`0x01B91190`, r5=0; SP=`0x0FEFFB80`. Saved caller LR at SP+`0xC0`=`0x0FEFFC40` is `AE50E0`; TOC save slot SP+`0x28`=`0x0FEFFBA8` contains main TOC. Existing `A34BB8`/`A34C48` confirm status.
- Empty C takes `AE5120`; allocation via interface slot `+0x2C`, OPD **`0x01968620` -> `B27658`**, descriptor **r11**, pre-LR **`AE50E0`** -> return **`AE514C`**. Arguments r3=`0x01B91190`, r4=`0x1000`, r5=4, SP=`0x0FEFFC30`; saved main TOC at `0x0FEFFC58`.
- `B27658` uses the same native manager mutex/translated allocator wrappers as the previous `0x2000` reserve. Its lock dispatch: descriptor r11=`0x019629E8` -> `A664E0`, pre-LR **`AE514C`** -> **`B276AC`**. Unlock dispatch: descriptor r10=`0x019629D8` -> `A662C8`, pre-LR **`B276C8`** -> **`B27708`**. Both execute with wrapper SP=`0x0FEFFB80`, mutex object r3=`0x01B91168`, saved caller LR `0x0FEFFC40=AE514C`, TOC slot `0x0FEFFBA8`. Lock timeout r4=0. Unlock consumes only r3.
- Given the preceding `0x2000` allocation ending at free chunk `0x01AD8D90`, this reserve should return **Q=`0x01AD8DA0`** and split next free chunk at **`0x01AD9DA0`**. These exact addresses are allocator inferences to confirm in execution. C becomes `{0x01B91190,Q,Q,Q+0x1000}`. No copy/free of an older buffer occurs.

## Four virtual key getters

All dispatches use singleton vtable `0x0186A628`, main TOC, descriptor **r9**, SP=`0x0FEFFC30`, and r3=`0x01B4C6E8`. Main TOC is saved at SP+`0x28`=`0x0FEFFC58`. These leaf getters save r30 in the red zone at SP-`0x10`; they do not allocate a frame or invoke other functions. Their r4/r5 values are volatile leftovers and are not arguments consumed by the getters.

| Slot | OPD / code | Incoming lifted LR -> original bctrl return | Returned key |
| --- | --- | --- | --- |
| `+0x10` | `0x01951DC8` / `7CEE00` | `AE514C` -> `AE522C` | `0x01B4C70F` |
| `+0x14` | `0x01951DD0` / `7CEE18` | `AE522C` -> `AE525C` | `0x01B4C70E` |
| `+0x18` | `0x01951DD8` / `7CEE30` | `AE525C` -> `AE528C` | `0x01B4C70D` |
| `+0x1C` | `0x01951DE0` / `7CEE48` | `AE528C` -> `AE52BC` | `0x01B4C70C` |

The original bctrl PCs are `AE5228`, `AE5258`, `AE5288`, `AE52B8`. These returned keys are **addresses in writable BSS, not executable callback code**. The registry stores opaque key/object pairs; it does not invoke them at this stage. Avoid describing this milestone as callbacks executing.

## Appends and sort

The four stack pairs are `{key,0x01B4C6E8}` at frame+`0x70`, `+0x90`, `+0x78`, and `+0x80`. Four direct calls to `AE47A0` use LRs `AE52D4`, `AE52E4`, `AE52F4`, `AE5304`.

With begin Q and reserved capacity Q+`0x1000`, every append takes `AE49C0`: copy two words, advance end by8, return. No call to allocation/free/string/error helpers is taken. Final end is Q+`0x20`.

`AE0BE8` receives r3=Q, r4=Q+`0x20`, r5=4 at direct-call LR **`AE5324`**. Count4 takes its in-function small-sort path `AE0CCC`. Comparison is signed int32 of each pair's first word, ascending. The initial descending keys all take prefix-rotation paths. **No `ADF198`, recursive sort, indirect dispatch or service call executes for count4.** Independent subagent review confirmed this branch.

Expected final table:

| Address | Key | Object |
| --- | --- | --- |
| Q + `0x00` | `0x01B4C70C` | `0x01B4C6E8` |
| Q + `0x08` | `0x01B4C70D` | `0x01B4C6E8` |
| Q + `0x10` | `0x01B4C70E` | `0x01B4C6E8` |
| Q + `0x18` | `0x01B4C70F` | `0x01B4C6E8` |

Validate `[0x01B92A60]=Q`, `[0x01B92A64]=Q+0x20`, `[0x01B92A68]=Q+0x1000`, the raw publication slot, flags, all eight table words, and allocator links/accounting. Existing name-registry and singleton state must also survive.

## Exact trace extension and completion evidence

Append these **21** entries after the 115-entry pre-`AE4490` checkpoint:

```text
AE4490 B0AF28 B25E58 A34BB8 A34C48 B27658 A664E0 16BB244
AC8578 AC8078 A662C8 16BB264 7CEE00 7CEE18 7CEE30 7CEE48
AE47A0 AE47A0 AE47A0 AE47A0 AE0BE8
```

`AE4D38` restores its saved LR=`7CF75C` and SP=`0x0FEFFD40` and returns. Caller `7CF5D0` then calls `7CEE78` at LR=`7CF764`; the global singleton is already published, so this adds exactly one trace entry and no subtree. Before calling `782468`, total is **137** and LR becomes `7CF770`. Seeing this boundary proves the whole `AE4D38` invocation returned; reaching the sort entry alone does not.

Relative to v0.9, expected total allocation requests are three (384,8192,4096 bytes), native lwmutex lock/unlock counts5/5, writer counts1/1, atomic attempts/successes1/1, mutex records12 and rwlock creations1. No guest backing arena beyond the existing pool is needed. Cleanup must still leave zero held/live native resources and zero backed image bytes after unload.

## Immediate caller continuation / next substantial allocator boundary

`7CF5D0` next calls `782468`, a second lazy singleton (slot `0x01B4BB04`, raw buffer `0x01B4BB09`, aligned object **`0x01B4BB0C`**, derived vtable `0x018682C0`). It calls existing `B0AF28` and `AE7768`; shared manager cache is populated, so `AE7768` skips `A35DB0`. Its manager-status dispatch now has incoming LR **`782518`** -> original return **`AE77E8`**. Existing guards that accept only initial `AE7854` must not silently cover this new context. The generic diagnostic singleton fields would also be overwritten by a second `AE7768`; preserve the first object's evidence explicitly if proceeding.

After this second singleton returns, `ADE478(first,second)` links `[0x01B4C6EC]=0x01B4BB0C`; no allocation/service occurs on the acyclic initial path. Caller then gets first singleton name/empty string through slots+8/+C and invokes virtual slot+`0x30`, OPD **`0x01966640` -> `AE42C8`**, return LR **`7CF82C`**. The original earlier getter return LRs are `7CF7D0` and `7CF804`.

`AE42C8` calls `AE3AC8` with first singleton/name/empty string. Because singleton+8 is zero, `AE3AC8` calls **`A941F8(size=0x38,interface=0x01B91190)`**, which dispatches interface slot **`+0x28`**, OPD **`0x01968628` -> `B27730`**. This is the next substantial allocation path (different default-alignment virtual method), followed by node construction and registration. It was not fully audited here; keep it a separate bounded milestone. No new OS syscall was identified before this allocation, but the current outer constructor cannot yet be claimed complete.

## Canonical complete-body hashes

Same exact regex convention as `generate_entry.py`, without trailing blank lines.

| Function | Chunk | SHA256 |
| --- | --- | --- |
| `AE4490` | 005 | `cbf78fcf689497981aa7a6e5e39f43915a5c61939335d3c16868bc0519933ffa` |
| `7CEE00` | 003 | `9fd561db2b583cdb2cfeb704ca1f6ffae6d6da4205ee5ad7e70595e00efc03b6` |
| `7CEE18` | 003 | `4bbfce2e9509142f695e92bb9839f37a9e1185a51c1c463eb0d83a3a5fd87bcd` |
| `7CEE30` | 003 | `fa4574dffa265bdddfd1db250b599d4df9884e171a39aa96aed975fa8591f657` |
| `7CEE48` | 003 | `b87b4b402ea3e6ddf9fdd82e1c1539f7dfadd25722bb77bac1c9893c4e67336e` |
| `AE47A0` | 005 | `33a028d170e7d913412e8fc0bf3b1851d81a84b0c830fcab6fcc7d13f4da6eb1` |
| `AE0BE8` | 005 | `ba8dd8cc8227d585d5971d51accb2b9be1b7bdb6b811531cb20a977ac6765927` |
| `782468` (follow-up) | 003 | `af9d4596b094caac293ee68647572c0060134bd8294680d300738f7e63f35c51` |
| `ADE478` (follow-up) | 005 | `677fc82671e9afab04dda559d797c580955ad654d2a1d822ba9ad82e47dbf7ff` |
| `AE42C8` (follow-up) | 005 | `a541885ac1be5ee398b4a50ab86bed3f36334fafb747c4982c4bcce03b674485` |
| `AE3AC8` (follow-up) | 005 | `962ada44f10b4462a7665ab51294521fb8a689f91d082b180594144c67d77360` |
| `A941F8` (follow-up) | 005 | `81eb110498d1540721ec9e7865184516329d938ba037fddf9f23cd22b6f94f77` |

Unreached assertion, allocation-failure/unwind, growth and larger-sort paths must remain strict stops. In particular `01674A5C` retains incompatible direct `vm_base` vector accesses. Do not import its flat-memory behavior into the segmented adapter merely because larger bodies reference it.
