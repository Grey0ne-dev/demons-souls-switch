# Wide streams and constructor thirteen complete — v0.27

The new `ds_entry_run_wide_streams` checkpoint completes original wide-stream
initialization and startup constructor thirteen. All eight narrow/wide stream
objects register. The wide space character is produced by original conversion
code, using the original TLS pointers and cross-TOC thunk. Execution stops
before constructor fourteen: OPD0194CF48 / code00745248 / TOC019B5040.

The boundary has 1,265 original function entries / 16,590 checked accesses,
thirteen completed constructors, 25 allocations and 66/66 guest locks. There
are eighteen real CRT mutex records and 58 thread-ID imports; no lock is held
at stop. All native/heap metadata clears, and image destruction releases all
backing. Guest memory stays 31,687,456 bytes under the existing 48 MiB cap.
The same 64 KiB heap arena backs 420 requested / 672 rounded live bytes.

Eight newly pinned full bodies bring production to 223, with all 215 previous
body hashes unchanged. Two wide indirect markers bring the total to 65 in
29 bodies. The new thunk's original four words and tail target are checked.
No native service or host character-conversion substitute is added. Mutex
registry capacity grows 16->32 and trace capacity 1,024->2,048, retaining strict
bounds and cleanup checks. All fifteen previous checkpoint APIs retain their
exact stops and counts.

All 9,926 ASan/UBSan/LSan checks pass: 589 full regressions, 549 clean startup
integration, 8,386 original-conversion oracle/ABI checks and 67 six-check fault
cases. A separate source/ELF model passes 567 state-word/metadata and twenty
instruction checks. Nine compiled lifter checks pass. The strict Switch build,
package CRC, manifest hashes and staged NRO equality pass. Fault instrumentation
was replaced by clean generation and tested again; it matches production.
All test/build jobs completed. Full source snapshots are preserved with the
sealed candidate and evidence in device-wide-streams-v0270/.

Candidate size: 530,629 bytes. SHA256:
2eed820a0564ed624dfa83021863fa1c3e8485f5413a8d084f0a633cee35e196.
v0.27 is installed through DBI MTP at 2026-10-28 19:03 UTC, with exact full
readback verified. The previous loader and log exactly match verified v0.26
and are backed up. Source, snapshot and evidence hashes remain intact. MTP
is released. Hardware execution is pending; installation is not a device pass.
Run Loader, wait for the checkpoint, A for a fresh second run, + to exit, then
reopen DBI MTP to retrieve/validate loader.log. No v0.27 hardware pass is claimed.

Next: audit constructor fourteen's wrapper00745248 and tail target00744E88.
Only the wrapper has been inspected; the target remains unreviewed/unselected.
Graphics, audio, scheduling, stream I/O, input and gameplay remain absent.
Source audit: WIDE_STREAMS_AUDIT.md. Handoff: CONTINUATION_STATE.md.
