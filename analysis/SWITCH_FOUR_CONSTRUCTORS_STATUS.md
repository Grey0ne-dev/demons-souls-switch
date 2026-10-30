# Four completed constructors — v0.13

The host path returns from four original static constructors and stops before
constructor five: OPD 0x01923818, code 0x00130D38, LR 0x016B84F4,
SP 0x0FEFFE00. It executes 354 function entries and 5,937 checked accesses.
This remains bounded startup; graphics, menus, audio and gameplay are absent.

Constructor four (0x009E54C8, initialization body 0x009E4F30) constructs
DLConsoleAppender metadata and initializes its DLAppender parent. It inserts a
17-byte type name, four opaque keys and SetChannel/GetChannel method descriptions.
The registered methods are not called. Names are ordered by length, so the new
17-byte record shifts the earlier 23-byte ImageProcessSceneEntity record.
The registry now contains three type records and twelve sorted key/object pairs.

The new path executes seven more pool requests: three 56-byte nodes, three
16-byte pointer vectors and one 64-byte method-record buffer. Rounding and pool
headers consume 416 bytes, leaving 0x5CBB8 in the existing 384 KiB pool. No new
guest-memory arena is added. Three additional recursive mutexes receive handles
0x1002 through 0x1004. Lock/unlock counts balance at 14/14, writer operations
remain 1/1, and every native registry is empty after stop or failure. The first
constructor's node/vector diagnostic fields retain their original addresses.

The generator pins 120 inspected original bodies, with 55 verified original
bctrl callsites across 24 functions. New full bodies use checked memory access;
unknown dependencies and error/fallback paths still stop explicitly. The
original source chunks were not regenerated or edited.

`ds_entry_run_four_constructors` validates the function suffix, stop ABI, pool
accounting, earlier objects, reordered name records, sorted keys, method
records, three new node graphs, pointer vectors and actual native handles.
The three-constructor API retains its previous checkpoint. Failed memory
validation now reports the first mismatching guest word.

Validation includes full startup regression, image/header mutations,
automatic/manual dispatch equivalence, the older three-constructor checkpoint,
four-constructor fresh reload and replay refusal, a corrupt method-registration
TOC after handle 0x1002, and a deliberate guest fault after real handle 0x1004.
Host checks use ASan, UBSan and leak detection; compiled lifter branch tests are
separate. The devkitA64/libnx build and archive verification pass. Build hashes,
test logs and the preserved candidate are in `device-four-constructors-v0130/`.

v0.13 passed twice on the actual Switch, including reload. Both attempts match
the complete 354-entry trace and checkpoint ABI, with 5,937 checked accesses,
balanced locks and zero native resources after cleanup. Final exit releases all
guest backing. The saved log passes 1,667 independent checks with no mismatches.
Evidence and the reproducible validator are in device-four-constructors-v0130/.
The next implementation frontier is constructor five, 0x00130D38 in generated
chunk 000; no new fifth-constructor bodies have been selected.

The v0.13 candidate was installed over the authorized DBI USB connection and
read back byte-for-byte: 407,749 bytes, SHA256
`936d067aa5e774d9f9f09ab5083c9493b9a701d761883038dba1ee35f1eb29a9`.
MTP was released after transfer and again after collecting the successful log. All 1,972 checks pass: 657 startup/backend/
dispatch/lifter plus 1,315 image-loader checks; 1,963 use sanitizers and leak
detection, with nine separate compiled branch cases. Device execution passed twice, including reload.
