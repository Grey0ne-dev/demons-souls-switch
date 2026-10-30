# Six completed constructors — v0.14

The host path returns from six original static constructors and stops before
constructor seven: OPD 0x01920778, code 0x000BF6E0, LR 0x016B84F4,
SP 0x0FEFFE00. It executes 424 function entries and 7,407 checked accesses.
This is bounded startup; no graphics, menus, audio or gameplay is implemented.

Constructor five (0x00130D38 / body 0x00130B68) registers DsShadowCam metadata
and initializes its parent object. Constructor six (0x0006B9E8 / body 0x0006B818)
registers ShaderContext_DS_FilterEntitySeed metadata and its parent. These are
type descriptions; no camera behavior, shader compilation or rendering runs.
Eighteen newly inspected source bodies, all in chunk 000, are hash-pinned.
There are 138 selected bodies and 55 original indirect sites in 24 functions.

Both new types use existing ELF backing. Allocation totals remain twelve,
native recursive mutex creations remain four, locks/unlocks balance at 14/14,
and writer operations remain 1/1. No new arena or OS service is added. The pool
still has 0x5CBB8 free bytes. All native resources are cleaned at the stop and on
dispatch failure; guest backing remains image-owned until destruction.

The name registry now has five records in length order: the new eleven-byte
shadow-camera name, the earlier two seventeen-byte names, the twenty-three-byte
scene name and the new thirty-three-byte shader-context name. Twenty opaque
key/object pairs are sorted globally, including the eight new keys that precede
older ones. Registered SetChannel/GetChannel method data from v0.13 is retained.

`ds_entry_run_six_constructors` validates the complete name/key records, both
new type objects and parent links, earlier method/node graphs, pool accounting,
native cleanup, exact function suffix and stop ABI. Shared registry validation
also preserves the old three- and four-constructor APIs.

All 686 host checks pass: 589 startup/backend/fault regressions, 20 earlier
three-constructor checks, 23 four-constructor checks, eight dispatch-equivalence
checks, 32 six-constructor checks, five registry-corruption checks and nine
compiled lifter branch cases. All except the nine branch cases use ASan, UBSan
and leak detection. Fault coverage includes corrupt fifth/sixth getter TOCs,
an executable but unselected target containing flat VM code, and corrupt key
data after all six constructors return. The last case must fail final memory
validation despite successful returns. Fresh reload passes; in-place replay is
refused. The strict devkitA64/libnx cross-build and ZIP CRC/content checks pass.

Candidate and source/test evidence: `device-six-constructors-v0140/`. The full
424-entry expected device trace combines the preserved v0.13 trace with the
new 70-entry sanitizer-tested host suffix. The latest observed hardware pass
was v0.13 at packaging time; v0.14 subsequently passed twice including reload.

The next unsupported target is constructor seven, 0x000BF6E0. It has not been
selected or reviewed. Larger key-registry sort paths remain unsupported if
future constructors reach their unselected dependencies.

The candidate was installed over DBI and read back with exact equality:
415,941 bytes, SHA256
`2a709b403c6ae60c1b46ed03f27a6839ebd9bc0434a446bc533f413e761a3321`.
The existing v0.13 log was saved before transfer; DBI was unmounted afterwards.
This transfer is not device execution evidence.

Observed hardware evidence: v0.14 passed twice, including reload. Both attempts
match all 424 function entries, 7,407 accesses, exact checkpoint ABI, balanced
locks and zero native resources. Final exit reports zero guest backing. The
saved log and reproducible validator pass 1,947 independent checks with no
mismatches; see device-six-constructors-v0140/device-result.json.
