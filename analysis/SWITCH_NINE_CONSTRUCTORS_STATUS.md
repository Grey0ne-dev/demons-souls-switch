# Nine completed constructors — v0.15

The host path returns from nine original static constructors, then stops before
constructor ten: OPD 0x01947270, code 0x0066A4A8, LR 0x016B84F4,
SP 0x0FEFFE00. It executes 561 function entries and 10,052 checked accesses.
This remains bounded startup; no graphics, menus, Lua scripts or gameplay run.

Three additional types and their parent metadata are initialized:

- Constructor seven, 0x000BF6E0 / body 0x000BF500: DsOTCollector. Its original
  instructions also increment the 64-bit guest counter at 0x01AD61A8 to one.
- Constructor eight, 0x005A9DC0 / body 0x005A9BF0: LuaMan. This registers the
  type description without entering the Lua runtime.
- Constructor nine, 0x008B89D0 / body 0x008B8600: DsMenuDlgSpellInfo. It writes
  both actual 0xFFFF sentinel halfwords, binds its static description and creates
  a metadata node and pointer vector. No menu behavior executes.

Twenty-seven newly reviewed bodies bring selection to 165 hash-pinned bodies.
The new chunk 002 is included in Make's generation dependencies. Constructor
nine adds three verified original bctrl sites, for 58 sites in 25 functions.
Unknown functions and larger-sort helpers remain explicit unsupported paths.
Source chunks were not regenerated or edited.

The name registry has eight records in length order: LuaMan (6), DsShadowCam
(11), DsOTCollector (13), the two earlier 17-byte names, DsMenuDlgSpellInfo (18),
ImageProcessSceneEntity (23), and the 33-byte shader-context name. The key
registry contains 32 globally sorted opaque key/object pairs. These are data
addresses, not callable callbacks. Thirty-two pairs is the largest range handled
by the selected sorter's in-body path; adding another type requires auditing
the currently unselected larger-sort helper.

Constructor nine requests 56 bytes for its node and 16 for its pointer vector.
Alignment and pool headers consume 112 bytes, leaving 0x5CB48 free. Totals are
14 pool requests, five actual recursive native mutex creations (last handle
0x1005), balanced 16/16 mutex locks/unlocks, and 1/1 writer operations. The fifth
handle is stored at 0x01AD9FF4. All allocations use existing ELF pool backing;
no new guest arena is added. Native resources are cleared on stop and failure.

`ds_entry_run_nine_constructors` validates the full registry records, new types
and parent links, earlier nodes and methods, the counter and sentinels, the new
node/vector/native handle, pool accounting, function trace and boundary ABI.
Older three-, four- and six-constructor APIs retain their checked boundaries.
Fresh reload passes; in-place replay is refused.

Fault coverage includes corrupt getter TOCs in all three new constructors,
corruption of the final (32nd) registry object after all nine returns, and a
guest-memory fault immediately after the fifth real native mutex creation.
All native registries must be empty in both late-failure cases. Final registry
corruption is rejected even when all nine constructors returned successfully.
Host validation uses ASan, UBSan and leak detection, plus separate compiled
lifter branch checks. The strict Switch cross-build and ZIP CRC/content checks
pass. Detailed build and test evidence is preserved in
`device-nine-constructors-v0150/`.

v0.15 passed twice on the actual Switch, including reload. Both complete
561-entry traces, result fields and cleanup pass 2,495 independent log checks
with no mismatches. The saved log, result and reproducible validator are in
`device-nine-constructors-v0150/`. v0.14 remains earlier hardware evidence. The next constructor audit is in `NEXT_CONSTRUCTOR_AUDIT.md`; constructor
ten and its larger-sort dependency have not been added to selection.

All 723 host checks pass: 589 startup/backend/fault regressions, 20 earlier
three-constructor checks, 23 four-constructor checks, 32 six-constructor checks,
eight dispatch-equivalence checks, 33 nine-constructor checks, five final-state
corruption checks, four late native-creation fault checks and nine compiled
branch cases. The first 714 use ASan, UBSan and leak detection.

The 432,325-byte candidate was installed over DBI and read back with exact
equality. SHA256:
`7b1eabb61abbd46edf7b8062d5c73e808537a3aa840d225ebbbbbff334abb23f`.
DBI was released after transfer and again after collecting the successful
device log. Native registries are empty after both attempts; final exit reports
zero guest backing.
