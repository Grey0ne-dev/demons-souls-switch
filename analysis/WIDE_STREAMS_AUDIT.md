# Wide streams and constructor thirteen — v0.27 audit

The bounded path completes original wide initializer0168BC68 and runtime
constructor015166F0 /01516590, then stops before constructor fourteen.
The next original table entry at018517F0 is OPD0194CF48, code00745248,
TOC019B5040. It is neither selected, entered nor counted at the checkpoint.
The constructor walker table is loaded through runtime TOC019D3BAC+50B4.

Eight complete original bodies are newly hash-pinned:0168BC68,0168E548,
0166CA50,0166FE88,00B17510,00010C50,0166C0E8,01664E60. All215 earlier
body hashes remain unchanged, giving223 bodies. Wide0168E548 adds two original
indirect return-PC markers;65 total in29 bodies. Only its fill virtual runs;
its destructor path remains outside this successful trace. Upstream chunks
are not modified or regenerated. Full merged00010C50 body retains unreachable
neighboring thunk sequences; they are not selected/executed as new entries.

The wide virtual uses original bctrl0168E68C / return0168E690, OPD01967FD0,
code00B17510 and actual main TOC019B5040. The dispatcher checks that exact
site/descriptor/code, facet60000240, r4=32, SP0FEFFB40, allocation counts22..25,
executable backing and no held lock. No general TOC acceptance is widened.
00B17510 calls original cross-TOC thunk00010C50, whose first four ELF words
are F8410028/3C420002/3842EB6C/4965B48C: save TOC, add1EB6C, tail branch
to0166C0E8. Both original wrapper and caller restore their saved TOCs.

The original conversion body consumes the space byte and writes its 16-bit
wide character. TLS getter0166CA50 returns0E000310;0166FE88 returns0E0003CC.
Their pointers are stored in the real32-byte facet at+18/+1C. The default TLS
conversion table is zero, selecting the original single/multibyte algorithm.
No host wide-character value, locale or conversion substitutes for game code.
The error path invokes original errno TLS getter01664E60 and stores8001001D.
The standalone oracle executes full original bodies (including merged thunk)
for ASCII, 16-bit multibyte values, alignment, split/resumed input, zero length,
null output, reset, malformed/unsupported sequences, error state and wrapper ABI.
This establishes those original-code paths, not unrestricted text I/O support.
A generated-code Clang parentheses warning is disabled only for generated code;
native code retains strict warning checks. No original instruction is rewritten.

Twelve additional allocations use exact size/caller/SP/TOC/sequence guards;
TOC is019D3BAC for every allocation:

| Ordinal | Size | Return PC | SP | Guest address |
| --- | --- | --- | --- | --- |
| 14 | 40 | 016A051C | 0FEFFC10 | 60000170 |
| 15 | 4 | 0168BD6C | 0FEFFCA0 | 600001A0 |
| 16 | 40 | 016A051C | 0FEFFC10 | 600001B0 |
| 17 | 4 | 0168BE38 | 0FEFFCA0 | 600001E0 |
| 18 | 40 | 016A051C | 0FEFFC10 | 600001F0 |
| 19 | 4 | 0168BEFC | 0FEFFCA0 | 60000220 |
| 20 | 4 | 01685FEC | 0FEFFAB0 | 60000230 |
| 21 | 32 | 0168E790 | 0FEFFB40 | 60000240 |
| 22 | 8 | 016884F0 | 0FEFFAC0 | 60000260 |
| 23 | 4 | 01685FEC | 0FEFFAB0 | 60000270 |
| 24 | 4 | 01685FEC | 0FEFFAB0 | 60000280 |
| 25 | 4 | 01685FEC | 0FEFFAB0 | 60000290 |

SPs follow original caller0FEFFD90 minus wide frameF0, stream frame160,
base/mutex frame90 or cleanup frame80 as appropriate. Request26 is rejected,
even with a correct size/caller/stack/runtime TOC. No free or growth path added.

Three wide buffer mutexes bring real CRT records to18. The shared bounded
native pool/result registry grows16->32 and function trace1024->2048. Bounds
and duplicate/ownership/teardown checks remain, with tests at new pool bounds.
This adds diagnostic capacity, not threading, waits or arbitrary native calls.
Guest backing stays31,687,456 bytes in the same48 MiB cap;25 live blocks use
420 requested /672 rounded bytes in the existing65,536-byte RW/non-X arena.

Four wide streams at01CE68A8/01CE68E0/01CE6918/01CE6950 register in original
slots5..8. All eight registry reference bytes equal1. Wide flags are0201,
except the third wide stream0203. Each fill is the original converted0020.
Seven new locale handles raise core references9->16. Narrow facet refs stay1;
wide facet refs1, cache01B93E84=60000240, facet ID2. The new cleanup node links
to existing600000F0. Original016884C0 registers a callback only when the list
was empty: free atexit slots stay78, not77; the existing callback is reused.
No registered cleanup/destructor is invoked.

A separate source/ELF model constructs the complete registry, 0x234-byte wide
aggregate, complete0x2A0-byte heap, counter fields and unchanged TLS words.
It also preserves the prior narrow aggregate from the sealed v0.26 model.
There are567 state-word/metadata checks and20 original-instruction checks.
The five original64-bit runtime counters become1; their flags remain0.
The original trace prefix966 is unchanged; reviewed new suffix299 gives1265.
All caller temporary snapshots have ended their lifetimes and are not treated
as live stack objects. The live locale/facet/stream/cleanup objects are checked.

Checkpoint stop38:13 completed constructors,1265 entries/16,590 accesses.
Boundary:r3=0,r4=0,r5=1,r6=0FEFFB24,LR016B84F4,SP0FEFFE00,
TOC019B5040,descriptor0194CF48,code00745248,last NID1BC200F4.
There are58 thread-ID imports and66/66 guest lock/unlock calls,held0.
Native metadata and heap ownership clear; destruction releases all backing.
All15 prior metadata/runtime checkpoint APIs retain exact stops/counts.

Fault fixtures cover all12 allocation contexts, wrong-sequence/cap rejection,
registry/reference/fill/handle/buffer/cleanup/counter/callback corruption,
next-target rejection, wide virtual target/argument/stack/TOC, conversion input
backing/output result, held registration lock and the last native mutex record.
Test-only hooks never enter production. Clean generation is restored afterwards.
Build, host and hardware results are tracked in SWITCH_WIDE_STREAMS_STATUS.md
and device-wide-streams-v0270/. Graphics, audio, scheduling, stream I/O, input
and gameplay remain absent; completing constructor thirteen is not full boot.
