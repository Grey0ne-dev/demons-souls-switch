# Locale object checkpoint — v0.21

`ds_entry_run_locale_object` follows the original locale consumer016896C0,
getter016891F0, TLS accessor016B7F30 and constructor01689180. These four complete
bodies are hash-pinned from chunk011; original chunks are unchanged. There are
196 selected bodies and61 original indirect-call markers in27 bodies.

The getter reads the actual TLS slot0E000430 (r13=0E007000, displacement-27600),
which is initially null. It acquires runtime lock zero and rechecks TLS. Its
52-byte operator-new request at LR016892DC receives60000040 after the existing
40/4-byte requests at60000000/60000030. Only this exact extra request is allowed
in the new mode. The older APIs retain their original stops and allocation ABI.

The constructor sets vtable018C7DE0, reference count1, zero flags/counts, empty
string storage and small-string capacity15. All128 bytes of the rounded heap
span are validated, including padding and the earlier embedded mutex. The string
member begins at60000058. Both the TLS locale slot and four-byte public handle
remain null: construction and publication have not completed.

Stop32 precedes cross-TOC string thunk01676088, after669 entries/12,314 checked
accesses. ABI:r3=60000058,r4=017041B8,r5=0,r6=01AD8EE0,LR016891DC,
SP0FEFFA30,TOC019D3BAC. Twelve constructors returned; thirteen is incomplete.
There are13 CRT mutex records,3 thread-ID calls,18 acquisitions/17 guest releases.
One native lock is held at the checkpoint; cleanup releases it and clears all
native registries and heap metadata. Guest bytes remain inspectable until image
destruction. The original lock wrapper's owner/count fields are checked too.

Heap accounting:3 allocations,requested96/live-rounded128/peak128 bytes,
no frees or failures,one65,536-byte RW/non-X arena. No extra physical backing
is needed. Total guest backing31,687,456 bytes within the50,331,648-byte limit.
In-place replay is rejected; fresh image repetitions recreate ownership cleanly.

Integration tests cover exact state/accounting,repeated fresh runs,old v0.20
checkpoint preservation,corrupted reference count,wrong third allocation size,
failure immediately after lock acquisition,and a non-null unmapped TLS locale
pointer consumed through original code. ASan/UBSan/LSan remain enabled.

The next unselected thunk01676088 stores the runtime TOC, adjusts it by-0x1EB6C
to019B5040 and tail-calls00F17448. Its pinned source contains additional
unreachable thunk sequences; review the original instruction range before
selecting it.00F17448 calls00010BA0 and00F172D0; the latter branches into string
capacity/assignment helpers00179118,00011720 and0001D570. The initial source
string at017041B8 is `*`; the later locale name at0174FDE0 is `C`. Do not seed
TLS or replace string operations to skip this work. Getter016B7F40 and subsequent
memcpy/memmove/new-handler/free paths remain unselected. General allocation,
waiting,threads,files,graphics,audio and gameplay are still outside this stage.

All943 sanitizer checks and nine compiled lifter checks pass. The strict Switch
build and ZIP CRC/content checks pass. Evidence/source hashes are frozen in
`device-locale-object-v0210/build-verification.json`. Candidate469,189 bytes,
SHA256950d3e593d11883f5b300ddceea9c82cc45a24388e480468485dbad405215d46.

v0.21 was installed through DBI MTP at2026-10-25 19:20 UTC; the full readback
matches the sealed candidate exactly. The previous binary and log matched the
verified v0.20 artifacts exactly and are preserved. See pre-install-verification.json
and transfer-verification.json.

v0.21 passed twice on the actual Switch, including fresh reload. All 2,993
independent checks pass with no mismatches: both full 669-entry/12,314-access
traces, exact ABI, locale scalar state, three allocations, native lock ownership,
cleanup and final zero guest backing. The clock advances 7,505,084 microseconds.
Post-run installed binary matches the sealed candidate; sealed source/evidence
hashes remain intact. Evidence:loader.log,device-result.json,and
postrun-verification.json. MTP is released; no further v0.21 device action is pending.
