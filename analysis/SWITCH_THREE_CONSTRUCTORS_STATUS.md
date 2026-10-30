# Three completed constructors — v0.12

The host path returns from the first three real static constructors and stops
before constructor four, descriptor 0x0195FFE0 / code 0x009E54C8. It executes
213 function entries and 4,141 checked accesses, with LR=0x016B84F4 and
SP=0x0FEFFE00. This is bounded startup; no graphics or gameplay is implemented.

v0.12 includes the complete v0.11 first-constructor work: both registries,
two linked singleton objects, five existing-pool requests, one private recursive
native mutex creation, a node and its pointer vector. The second constructor
sets its actual static vtables/fields. The third constructs ImageProcessSceneEntity
metadata and its parent, inserts its 23-byte name in the existing buffer,
appends four more opaque key/object pairs and sorts all eight pairs. It performs
no extra allocation or OS service. These keys are writable BSS addresses and
are not executed callbacks.

Both registries are checked against actual guest memory: name count two,
key count eight. Native mutex lock/unlock totals remain 7/7, writer operations
1/1; heavyweight creation count one. All native resources are cleaned at the
bounded stop, with guest backing still owned until image destruction. Fresh
reload reproduces the checkpoint and in-place constructor replay is refused.

104 full source bodies are hash-pinned. Production uses verified original
indirect return-PC annotations and a typed selected-function dispatcher. See
INDIRECT_DISPATCH_STATUS.md for exact limits and equivalence validation.
The unimplemented NULL-pool allocator fallback remains an explicit stop.

The Switch cross-build passed. Host validation includes the full regression
suite, native/provider/attribute/cleanup faults, direct execution tests of linked
branch lowering, automatic/manual memory equivalence, invalid callsite refusal,
three-constructor memory validation, fresh reload and descriptor corruption.
All 630 checks passed: 589 regression/backend/fault checks, eight automatic/manual
equivalence checks, four invalid-callsite checks, 20 three-constructor checks and
nine linked-branch execution checks. Build hash and source fingerprints are in
analysis/device-three-constructors-v0120/build-verification.json.

Device installation/execution is pending because DBI MTP is currently unavailable.
The latest hardware pass remains v0.10, twice including reload. The preserved
v0.11 first-constructor candidate was built/tested but never installed. Work
continues with inspection of constructor four while awaiting the MTP server.
