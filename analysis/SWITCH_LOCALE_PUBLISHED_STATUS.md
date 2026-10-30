# Locale strings and publication — v0.22

`ds_entry_run_locale_published` executes the original string constructor and
publishes the allocated locale object through the real TLS setter, both global
slots and the four-byte public handle. The string member first receives `*`,
then `C`. Its size is 1, capacity 15, flags 63 and reference count 3.
No string bytes, TLS pointers or completion state are supplied by host overrides.

Seven complete original bodies are added: 01676088, 00F17448, 00F172D0,
00179118, 00011720, 01666594 and 016B7F40. All 203 bodies are hash-pinned;
the original chunks remain unchanged. Both cross-TOC thunks save the incoming
TOC and tail branch after four instructions. Their exact ELF words, branch
targets and reachable ranges are checked by the generator and recorded in
provenance. Neighboring unreachable thunk sequences remain unchanged.
The 61 indirect-call markers in 27 bodies are unchanged from v0.21.

Stop 33 precedes the next operator-new 0168ABD0 invocation, after 697 entries
and 12,492 checked accesses. ABI: r3=40, r4=01AD58D0, r5=01CE6510,
r6=01CE6500, LR=016A051C, SP=0FEFFB60, TOC=019D3BAC. Twelve constructors
have returned; thirteen remains incomplete. The next allocation is not served.

Object 60000040 is published at TLS 0E000430, handle 60000030 and globals
01CE672C/01CE6730. All 128 rounded heap bytes and the 120-byte narrow-locale
aggregate are checked, including the next embedded mutex's base vtable and null
allocation output. All earlier metadata is validated by the shared checkpoint
validator. Older APIs preserve their exact trace counts and boundaries, including
the v0.21 stop before selected string thunk 01676088 is entered or counted.

The three requests remain 40/4/52 bytes: 96 requested, 128 live-rounded bytes,
one 65,536-byte RW/non-X arena. Total backing remains 31,687,456 bytes under
the 50,331,648-byte cap. There are 13 CRT mutexes, five thread-ID imports and
19/19 native lock acquisitions/releases. Original wrapper recursion executes
without adding native recursive-lock support. No native lock is held at stop;
native registries and heap metadata clear on stop and failure. Image destruction
releases all backing. In-place replay is refused; fresh repetitions pass.

Host validation covers publication, complete heap contents, resource accounting,
fresh repetitions and all ten metadata/runtime checkpoints from v0.12 through
v0.21. Fault fixtures corrupt the locale string or TLS publication, pass unmapped
copy input/output, corrupt a TLS pointer consumed by original code, force an
unsupported string-growth request, and change the next allocation size.
Held-lock failure cases clean up native ownership and all guest backing.

The exact original 404-line copy body passes 7,681 standalone sanitizer checks:
30 lengths from 0 through 4,096, every source/destination alignment modulo eight,
both address orders, all surrounding bytes and preserved ABI registers. These
are nonoverlapping copy cases; they do not establish memmove semantics.

ASan, UBSan and leak detection remain enabled. The strict devkitA64/libnx build,
package CRC and archived binary/input hashes are checked before sealing.
Versioned evidence and the independent device validator are in
`device-locale-published-v0220/`. All 8,383 sanitizer checks and nine compiled
lifter checks pass. The clean exploration executable was restored after fault
testing and again passes its 78 checks; those repeat checks are not counted twice.

The sealed candidate is 481,477 bytes, SHA256
`8cae8c3db80720a2be90183520f6a80f6576abe3afea9208343154bea00adcf8`.
It was installed through DBI MTP at 2026-10-26 08:32 UTC; exact full readback
matches the candidate. The prior installed binary and log match verified v0.21
and are preserved. MTP is released. Sealed source/evidence hashes remain intact.
v0.22 passed twice on the actual Switch, including fresh reload. All 3,121
independent checks pass with no mismatches: both full traces and exact ABI,
locale string/TLS/global/handle publication, heap accounting, balanced native
ownership, metadata cleanup and zero guest backing at exit. The clock advances
14,393,912 microseconds. Post-run installed binary matches the sealed candidate;
sealed source and evidence hashes remain intact. The log and validation result
are preserved as loader.log/device-result.json; postrun-verification.json records
binary/hash verification and released MTP. No further v0.22 device action is pending.

Next work: audit additional runtime allocation callers and the remaining locale
aggregate construction. String growth, guest free/new-handler/exception paths,
memmove, waiting, guest threads, files, graphics, audio and gameplay remain
unsupported. This checkpoint does not establish full boot or playability.
