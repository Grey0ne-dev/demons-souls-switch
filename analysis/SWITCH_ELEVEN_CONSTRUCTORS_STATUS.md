# Eleven constructors — v0.17

The native Switch candidate completes eleven original static constructors,
then stops before constructor twelve: OPD 0x01935910 / code 0x003F46B0.
The path executes 603 function entries and 11,044 checked memory accesses.
This remains bounded startup; graphics and gameplay are not implemented.

Constructor eleven's wrapper 0x005FD720 and initializer 0x005FD628 are selected
from chunk 003. Both complete bodies were reviewed and hash-pinned; the existing
0x00DBA100 initializer supplies the only direct dependency. The normal path sets
three eight-byte static objects and one sixteen-byte composite at
0x01B48C44..0x01B48C6B. All ten words, including the original vtables and zero
payloads, are checked. The final ABI is r3=0x01B48C64, r4=0x0FEFFDE8,
r5=0x0FEFFDE0, r6=0, LR=0x016B84F4, SP=0x0FEFFE00.

Selection now contains 177 original bodies. Verified indirect annotations
remain 58 sites in 25 functions. Original translated chunks were not edited.
Unknown later functions and unsupported services remain explicit stops.

All previous object, node, method, name and sorted-key validation remains
active. Registry totals stay at nine names and 36 opaque key/object pairs.
Resource totals stay at 14 existing-pool requests, five native recursive mutex
creations, 16/16 mutex operations and 1/1 writer operations. Pool free bytes
remain 0x5CB48. No new backing arena or native service was added. Native
registries are empty at the checkpoint and tested failures.

Host coverage includes repeated fresh-image validation, refusal of in-place
replay, corrupt constructor eleven and twelve TOCs, a null static destination
which faults at the original checked write, and corruption of the composite's
second vtable after eleven actual returns. Final-state corruption is reported
at 0x01B48C64. Earlier checkpoint APIs and automatic/manual dispatch equivalence
retain their expected results. The original larger-sort matrix was unchanged;
its 126-case / 512-pair evidence remains in the preserved v0.16 directory.

Evidence and candidate: device-eleven-constructors-v0170/. The preserved v0.17
candidate was installed through DBI MTP on 2026-10-25 and read back byte for byte:
440,517 bytes, SHA256 a1c48cb43f96e8df39a7aae589ab2435db17cbeedd84922ce7d252b7c4b349d8.
The retrieved prior log and installed NRO were confirmed as v0.16. The transfer
receipt is transfer-verification.json. v0.17 then passed twice on the Switch,
including fresh reload: all 2,665 independent log checks pass with no mismatches.
Both attempts match all 603 entries, 11,044 checked accesses, the stop ABI and
resource totals. The hardware clock advances by 6,573,150 microseconds across
reload. Native resources clear on both attempts and final guest backing is zero.
Evidence: loader.log and device-result.json. The installed NRO was retrieved
again after execution and still matches the preserved candidate byte for byte.
