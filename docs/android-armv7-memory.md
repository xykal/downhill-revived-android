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
