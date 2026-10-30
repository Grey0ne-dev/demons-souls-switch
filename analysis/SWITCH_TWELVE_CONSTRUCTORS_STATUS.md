# Twelve constructors — v0.18

The bounded checkpoint completes twelve original static constructors, then
stops before constructor thirteen: OPD 0x019A3EA0 / code 0x015166F0 / TOC
0x019D3BAC. It executes 641 entries and 12,134 checked memory accesses. The
last registers are r3=0x01B42878, r4=0x01BB6060, r5=0, r6=0x01AD8EE0,
LR=0x016B84F4, SP=0x0FEFFE00. Stop enum is 28. No thirteenth body is selected.

Constructor twelve registers ImageProcessDrawEntitySeed (26 bytes), inserting
the tenth name and four opaque keys for forty sorted pairs. Root 0x01B42878
has vtable 0x0185D620, parent 0x01BB6060, allocator 0x01B91190, name/signature
pointers 0x016DBD50/0x016DBD18 and zero node/method fields. The original parent
getter initializes vtable 0x0187AF88 and allocator 0x01B91190. All root/parent
words and initialization flags are checked alongside all earlier state.

Nine full source bodies were reviewed and hash-pinned: 003F46B0, 003F44E0,
003F3F10, 003F3F18, 003F3F30, 003F3F48, 003F3F60, 003F3F78 (chunk001), and
00CE3DF0 (chunk006). The approved total is 186 bodies. Existing indirect-site
annotations remain 58 sites in 25 bodies; no original translated chunk changed.
The annotation option no longer enables experimental bodies; an independent
--include-frontier option is required for any future exploratory selection.

The next constructor's original runtime TOC differs from the game TOC. Its
exact OPD/code/TOC plus twelve completed returns are validated before recording
the stop. Unknown executable targets, modified OPDs or TOCs cannot establish
this production checkpoint. Arbitrary non-main-TOC dispatch remains rejected.

No extra pool allocation, arena or native service is needed. Resource totals
remain 14 existing-pool requests, five native recursive mutex creates, 16/16
mutex operations, one writer acquire/unlock and one native atomic success.
Pool free bytes remain 0x5CB48. Both the 36- and 40-pair sorts execute the
original partition helper. Native registries clear at bounded stop and faults.

All 833 host checks passed with ASan, UBSan and leak detection, plus nine
compiled lifter branch/link cases. Host validation covers fresh-image repeats,
replay refusal, invalid twelfth
constructor and getter TOCs, wrong thirteenth TOC/code (including substitution
of the game TOC), an unbacked name read, corrupt final parent/key payloads, and
failure in the actual 40-pair partition. Existing startup and older checkpoint
regressions and automatic/manual dispatch equivalence are also run under ASan,
UBSan and leak detection. Exact logs and counts are frozen by seal_candidate.py
in device-twelve-constructors-v0180/ after all required checks complete.

The strict devkitA64/libnx build and ZIP CRC/content verification passed. The
sealed candidate is 444,613 bytes, SHA256
44c730853e1f9f52e743f968d5dc7efc739d51f7f300dfc6d2bacdf81a88d210.
It was installed over DBI MTP on 2026-10-25 17:55 UTC, and a complete readback
matches byte for byte. transfer-verification.json records the transfer; the
v0.17 installed binary and log were checked before replacement.

v0.18 passed twice on hardware, including fresh reload: all 2,817 independent
checks pass. Both runs match all 641 entries, 12,134 accesses, exact ABI and
resource totals. The clock advances by 4,871,824 microseconds across reload;
native registries clear on both runs and final guest backing is zero. Evidence:
loader.log and device-result.json. An initial log had two passing checkpoints
but lacked the exit marker; it is retained separately as incomplete-exit-loader.log
and incomplete-exit-device-result.json, without weakening validation.
Graphics, files, audio, scheduling and gameplay remain outside this bounded
initialization slice.
