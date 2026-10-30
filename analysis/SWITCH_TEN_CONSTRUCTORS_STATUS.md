# Constructor ten and larger registry sorting — v0.16

The host path completes ten original static constructors, then stops before
constructor eleven: OPD 0x01943CD0, code 0x005FD720, LR 0x016B84F4,
SP 0x0FEFFE00. It executes 600 function entries and 11,007 checked accesses.
This remains bounded startup; file operations, graphics and gameplay do not run.

Constructor ten (0x0066A4A8 / body 0x0066A2D8) initializes FileCap metadata and
its parent through 0x005720A8. Its seven-byte type name brings the name registry
to nine entries; four additional opaque keys bring the key registry to 36.
The original sorter 0x00AE0BE8 now calls its original larger-range partition
helper 0x00ADF198. The helper's complete 502-line translated body was reviewed:
it uses checked scalar accesses and has no further function dependencies.
The production path retains the original comparisons and key/payload swaps.
No host sorting substitution is used in the Switch build.

Ten newly reviewed bodies bring selection to 175 hash-pinned bodies. Indirect
sites remain 58 in 25 functions. Unknown later functions remain explicit stops.
No original source chunk was edited or regenerated.

Allocation and OS-service totals are unchanged: 14 existing-pool requests,
five actual native recursive mutex creations, balanced 16/16 mutex locks and
1/1 writer operations. The pool still has 0x5CB48 free bytes. No additional
guest arena is added. The new type object, parent links, all nine name records
and all 36 sorted opaque key/object pairs are checked. Native registries are
empty after the checkpoint and tested failures.

Metadata checkpoint constants now live in explicit profiles for constructors
3, 4, 6, 9 and 10, including expected function/access counts, stop ABI and resource
totals. Older APIs preserve their boundaries. Memory validation still reports
the first corrupt word when a fault fixture also adds a checked access.

The host-only sort matrix replaces the input of the actual tenth-constructor
outer sort call, within the existing 4096-byte registry allocation. It runs the
original sort and partition bodies unchanged. This is synthetic sorting
validation, separate from the real 36-key production checkpoint.

All 126 matrix cases pass 1,260 checks with ASan, UBSan and leak detection:

- Counts 0, 1, 2, 31, 32, 33, 36, 40, 41, 42, 64, 128 and 512.
- Ascending, descending, equal, duplicate signed extremes, deterministic random
  signed keys, and organ-pipe patterns.
- Positive sort depth and forced depth-zero fallback for counts above 32.
- Signed output order, exact key/payload multiset against a host oracle,
  unchanged adjacent guard words, expected partition/fallback use, and complete
  native/guest cleanup. Duplicate keys do not assume stable ordering.

The normal ten-constructor checkpoint passes fresh reload and refuses in-place
replay. A corrupt FileCap getter TOC is rejected before the tenth return.
A null partition input stops at a checked guest read; corrupting the 36th
registered object after all ten returns fails final validation. Both failure
paths release all native registries. Earlier checkpoints and automatic/manual
dispatch equivalence remain covered.

The strict devkitA64/libnx cross-build and ZIP CRC/content checks pass. Build,
source and test evidence is preserved in `device-ten-constructors-v0160/`.
The full expected device trace extends the validated v0.15 561-entry reference
with the new 39-entry host suffix. v0.16 subsequently passed twice on the actual
Switch, including reload. Both runs execute ADF198 once and match all 600
entries, 11,007 accesses, registry counts, ABI and resource cleanup. All 2,653
independent device-log checks pass with no mismatches.

All 2,005 host checks pass: 589 full regressions, 20 three-constructor,
23 four-constructor, 32 six-constructor, 33 nine-constructor, eight dispatch
equivalence, 22 ten-constructor, five final-state faults, four partition-input
fault checks, 1,260 matrix checks and nine compiled lifter branch cases. The
first 1,996 use ASan, UBSan and leak detection.

The 440,517-byte candidate was installed over DBI and read back with exact
equality. SHA256:
`fe8f07b84343ac43738afd282432b4067063d7a962a1a5b9e95c495a6e20bec7`.
DBI was released after transfer and again after saving the successful device
log. Native resources are empty after both attempts; final exit releases all
guest backing. Synthetic 512-pair matrix results remain host-only evidence.
