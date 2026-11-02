# Android armv7 guest address-space investigation

Status: **not bootable on the reported Android 13 32-bit device**. This is not
an APK fix announcement. The early-reservation ownership bug is corrected, but
that does not eliminate the need for a contiguous `0x70e10000`-byte span.

In the supplied process map the app binary ends at `0x05972000`, and Android's
boot-image mapping starts at `0x6fc1c000`. The gap is `0x6a2aa000` bytes
(1698.66 MiB); the requested span is 1806.06 MiB. A different reservation time
or ASLR layout might change details, but the displayed gap alone cannot hold it.
The log's span size refers to virtual address space, not committed physical RAM
or disk. The selected content URI does not establish that all ISO I/O works.

The earlier startup code reserved early and then reserved a second time instead
of reusing the first reservation, leaking both the span and its RAM fd on
success. `test_reservation.c` tests reuse, retry after an early failure, normal
allocation, and cleanup without needing proprietary game files. It exercises
the shared policy with a fake OS backend, not Android kernel behavior.

Potential direction: a segmented mapping that preserves guest pointer values.
The supplied map has a large free low interval and another free interval above
Android's boot image. A segmented allocator could try to reserve the live
regions at matching relative offsets, without reserving the huge unused gap
between the high RAM mirror and the scratchpad. This is only a design hypothesis:
Android ASLR, kernel address-space constraints and page alignment vary.
Reservations must be race-safe; do not use `MAP_FIXED` against unowned mappings.

**Safety invariant:** unreserved holes can contain Android runtime or app
mappings. The present implementation relies on inaccessible reservations to
trap invalid guest accesses, and `guest_base + guest_address` is used by both
wasm2c fast paths and native services. Merely omitting the hole is unsafe: a bad
guest pointer could silently read or overwrite unrelated process memory. Any
segmented solution must audit and guard every guest access path, including the
wasm2c hot/cold split and bulk operations (`memory_access.h`,
`prepare_wasm2c.py`), `GUEST`/`GADDR`, native bridge and host imports, commit,
growth, cleanup and fault handling. Preserve RAM mirror aliasing and wasm
bounds behavior, including crossing spans. Add negative and boundary tests on
32-bit plus real-device testing before publishing an APK. A passing build or a
64-bit mock test is not evidence of phone boot or graphics compatibility.

## Experimental reservation prototype

`split_reservation.c` is integrated only behind an experimental flag, not in normal APK builds. It plans aligned
pairs of free gaps, snapshots `/proc/self/maps`, and claims both intervals with
non-destructive `mmap` hints, undoing the first claim if the second fails or
lands elsewhere. A maps snapshot is not a lock; callers must retry after races.
The planner's range predicate is a building block; the experimental wasm2c
adapter and bridge use their own shared split-range predicate.

On a synthetic fixture representing the player's reported two gaps, the
planner finds base `0x059fc000`. Including 16 KiB safety reservations adjoining
both sides of the omitted hole, the proposed low reservation is
`[base, base + 0x3a004000)` and the high reservation is
`[base + 0x6fffc000, base + 0x70e10000)`. This shows arithmetic feasibility
for that one captured layout, not that `mmap` will succeed on the actual phone.

Linux tests compare the gap planner to 10,000 brute-force random layouts, check
range boundaries and actual `/proc/self/maps` exclusion, and verify that claim
and rollback do not overwrite occupied low/high addresses or unmap a sentinel
in the hole. CI cross-compiles the prototype for armv7. These tests do not
validate every wasm/native access path, Android runtime behavior, game boot,
content URI I/O, or rendering. The full audit and 32-bit runtime tests above are
still required before enabling it or distributing an APK as fixed.

## Additional 32-bit allocator defects

32-bit ARM tests exposed another bug independent of fragmentation: the shared
loop skipped its last region, which is the inaccessible guard on 64-bit but
contains the scratchpad and module on 32-bit. The fix processes all 32-bit
regions and rejects memory growth beyond the actually reserved span, before
calling any decommit on failure. A mocked ownership/commit test and a real POSIX
`mmap` test both pass as 32-bit ARM Linux binaries under QEMU, as well as on
Linux x64. A negative control confirmed the prior armv7 code fails the module
commit assertion. QEMU Linux is not Android; these do not prove phone boot.
The allocator still cannot reserve a contiguous span on the reported phone.

## Experimental allocator and generated-wasm guard

`DHD_EXPERIMENTAL_SPLIT_MEMORY` now gates an experimental split allocator and
wasm2c adapter. It is **not enabled by default** in the Android APK build; the
explicit experimental workflow dispatch builds an unverified, separately named
artifact for testing only. It skips the
unowned gap during mapping and teardown, reserves guard pages beside the gap,
rejects host commits across it, and checks wasm hot, cold and bulk addresses
before dereferencing them. The generated native bridge checks buffer ranges and
bounds strings to their segment. Native `GUEST` rejects starting addresses in the
hole on any thread, but it checks only one byte, not later pointer arithmetic or
buffer lengths. `GADDR` and native host imports still require a full audit. Never label it a
verified or safe fix until the audit and device testing are complete.

`test_split_module.py` uses the real experimental allocator and a tiny WAT
fixture compiled by WABT and prepared by the project's wasm2c adapter. In Linux
x64 and under 32-bit ARM Linux QEMU it tests module startup, RAM aliasing,
scratchpad, growth, scalar hot/cold/offset accesses, SIMD v128 load/store,
fill/copy, empty spans,
rejected hole/boundary accesses, bridge buffers and strings, native `GUEST` hole
rejection in a child process, and cleanup. A real writable sentinel is mapped
in the hole before module instantiation and checked after cleanup. A negative
control deliberately disables the wasm guards and fails the hole-trap check.
These are synthetic tests, not proof that Android or the full game boots.

The experimental POSIX host now rejects page-protection requests that span the
unowned hole and declines to offer faults in the hole to the guest fault
callback. The generated-module fixture verifies these using a writable sentinel
and a separate child faulting in that sentinel. This does not replace the
remaining native pointer-length and `GADDR` audit.

The fixture runs with optimized native Clang and 32-bit ARM Linux GCC and Clang
under QEMU, including a guard-disabled negative control. This exercises Clang's
real `__builtin_assume` at `-O3` as well as SIMD via SIMDe, but is not a real
Android test. Reservation takes fresh map snapshots and advances its candidate
floor after a failed non-destructive claim (up to 32 attempts). Android logs
whether the early split attempt was ready and logs the base of a successful split
reservation. Full native span checking remains unfinished; the experimental APK
may abort, crash, or corrupt host state on a bad native access.
