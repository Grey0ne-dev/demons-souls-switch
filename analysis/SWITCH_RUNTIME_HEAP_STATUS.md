# Runtime locale / heap boundary — v0.19

The checkpoint completes twelve static constructor returns, enters constructor
thirteen at015166F0 with the original runtime TOC019D3BAC, and stops before its
first operator-new request at0168ABD0. Thirteen has not completed. Exact ABI:
r3=40,r4=0,r5=0,r6=01AD8EE0,LR=016A051C,SP=0FEFFB60. Stop enum29.
The checked path executes652 entries and12,206 memory accesses.

Four reviewed and hash-pinned bodies were added:015166F0/01516590 in chunk010,
01686CE8/016A0500 in chunk011. Production total190. The full500-line narrow
locale initializer retains its original branches and strict later boundaries.
Its two bctrl instructions are independently verified from the original ELF,
bringing annotation totals to60 sites in26 functions. Original chunks unchanged.

The runtime uses an existing CRT mutex and returns it unlocked:17/17 native
lock operations. Counter01AD6020 changes fromFFFFFFFF to1. Locale object
01CE64E8 gets original vtable018C1BF0, while its mutex pointer01CE64EC stays0.
The wide-locale counter01AD6088 and five subsequent64-bit initialization counters
remain unchanged. Ten names/forty sorted keys and all earlier objects, pool
accounting and native resources are validated by the shared metadata predicate.
Resource totals remain14 existing-pool requests,five native recursive creates,
one writer acquire/unlock and one native CAS; no new backing or allocation.

The explicit heap-checkpoint mode validates its request/stack/TOC before
accepting the stop. Ordinary exploration still reports unsupported allocation.
Tests cover fresh repeats,replay refusal,wrong runtime TOCs,null locale output,
an alternate counter branch reaching an unselected wide initializer,corrupt
final locale counter,wrong allocation size and a checked fault while the actual
runtime mutex is held. Native teardown is required on every failure.

All866 ASan/UBSan/leak checks and nine compiled lifter branch/link cases passed.
The strict devkitA64/libnx build and ZIP CRC/content checks passed. Evidence and
source hashes are sealed in device-runtime-heap-v0190/. Candidate452,805 bytes,
SHA2563843cae625a4607a27cfe6deb7fe146132357e53fcbe8e49902a97e1b87c1ae9.
It was installed via DBI MTP on2026-10-25 18:17 UTC, with exact readback verified.
The prior v0.18 device log and installed binary were checked before replacement.
v0.19 passed twice on the Switch, including reload: all2,873 checks pass.
Both652-entry/12,206-access traces match, two thread-ID calls and17/17 locks
are confirmed, the locale state matches, and final guest backing is0. The clock
advances by8,655,754 microseconds. The post-run installed NRO still matches the
sealed candidate. Evidence:loader.log and device-result.json.

The external validator initially retained v0.18's expected single thread-ID
call. Original import016BB2E4 occurs at frozen trace indices44 and646, so its
expected count was corrected to exactly2. The original validator/result are
preserved as validate_device_pre_correction.py and pre-correction-device-result.json.
validator-correction.json records both hashes and the reasoning; the sealed
build manifest, candidate and expected trace were left unchanged.
v0.18 passed twice on the Switch with2,817 checks,
including reload,full trace,clock advancement and final zero-backed-byte cleanup.

Next: reviewed operator-new0168ABD0 calls0167AC88, a preexisting ds_malloc host
override in the original lift. Neither is selected. Implement a separately
backed,budget-charged guest heap and its ownership/alignment/free behavior before
serving that path; never return a host pointer or fabricate guest backing.
