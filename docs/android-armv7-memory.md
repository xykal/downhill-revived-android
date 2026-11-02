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

`split_reservation.c` is **not integrated with the game**. It plans aligned
pairs of free gaps, snapshots `/proc/self/maps`, and claims both intervals with
non-destructive `mmap` hints, undoing the first claim if the second fails or
lands elsewhere. A maps snapshot is not a lock; callers must retry after races.
The prototype's range predicate is only a building block: it is not applied to
wasm2c or native guest accesses.

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
validate wasm/native access protection, Android runtime behavior, game boot,
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
