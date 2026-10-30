# First complete constructor checkpoint — v0.11

The translated first static constructor now returns to its original CRT walker.
The host path reaches the next constructor, descriptor 0x01947390 / code
0x0066D688, after 176 entries and 3,647 checked accesses. LR=0x016B84F4,
SP=0x0FEFFE00. The original constructor descriptor remains 0x01951EA8.
This is startup execution; graphics, gameplay and performance are unverified.

92 full pinned bodies. Both name/key registries run; four opaque key/object
pairs sort in ascending order. These keys are BSS addresses, not executed
callbacks. A second singleton at 0x01B4BB0C is constructed and linked to the
first. Five real pool requests (384,8192,4096,56,16 bytes) create two registry
buffers, a 56-byte node at 0x01AD9DB0 and its pointer vector at 0x01AD9E00,
along with the original manager range allocation. Rounded allocator headers,
free-list links, accounting, object fields and original registry contents are
checked. No additional guest arena is allocated.

Syscall 100 is sys_mutex_create, verified against the pinned local runtime's
sys_mutex.h and LV2 table. The full 40-byte attribute span and four-byte output
are checked before native creation. Accepted semantics: protocol=2,
recursive=0x10, private pshared=0x200, adaptive=0x2000, zero IPC key/flags,
empty name. Padding and seven uninitialized trailing name bytes are ignored.
The eight-slot native registry creates libnx RMutex objects on Switch and
PTHREAD_MUTEX_RECURSIVE objects on Linux. The first opaque handle is 0x1001,
written at node+0x34 (0x01AD9DE4). This service implements creation only;
heavyweight lock/wait/destroy syscalls and priority waiter scheduling remain
unsupported. Bounded-stop cleanup destroys all created native objects.

Native lwmutex locks/unlocks balance 7/7, writer operations 1/1. Twelve guest
lwmutex records, one rwlock and one heavyweight mutex are cleaned at stop.
Guest backing remains 31,621,920 bytes until image destruction. Constructor
replay into a modified image is refused; fresh reload reproduces the result.

All 583 host checks passed: 415 main tests, 123 service/fault fixtures,
30 atomic and 15 rwlock-backend checks, with ASan/UBSan/leak detection.
Faults include malformed kernel output/attribute spans, unsupported shared
attributes, native provider failure, a guest fault after actual kernel creation,
wrong method targets/TOCs, and a corrupted next-constructor TOC. Native registry
capacity/recreation and complete final memory state are checked. The Switch
cross-build passed with strict warnings and existing generated-code suppressions.

v0.11 is built and packaged; device installation/execution is pending.
Candidate and build hash: analysis/device-first-constructor-v0110/.
The latest hardware pass is v0.10 (two attempts including reload).

Next: execute the second constructor (two new small bodies plus the existing
destructor helper), then audit the third constructor's metadata registration.
The new node's mutex creation does not establish later mutex operations,
threading, asset loading, graphics or gameplay.
