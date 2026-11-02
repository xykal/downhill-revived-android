/* Split reservation planner and claims, used only by the opt-in experimental allocator. */
#ifndef DHD_SPLIT_RESERVATION_H
#define DHD_SPLIT_RESERVATION_H
#include <stddef.h>
#include <stdint.h>

/* Half-open free interval in the 32-bit host address space. */
typedef struct { uint64_t begin, end; } DhdAddressGap;
/* All units are bytes, with a power-of-two page alignment. The reservation
   owns [base,base+low_size) and [base+high_offset,base+high_offset+high_size).
   Holes between them are NOT owned and MUST NOT be accessed by the guest. */
typedef struct {
    uint64_t low_size, high_offset, high_size, alignment;
} DhdSplitLayout;

/* Find the first aligned base whose two regions fit. Returns 1 and stores
   the base; returns 0 if no candidate or invalid input. This is only a plan:
   another thread can change mappings between planning and reservation. */
int dhd_split_find(const DhdAddressGap *gaps, size_t n, DhdSplitLayout layout,
                   uint64_t *base);

/* Strict guest range guard; no address translation occurs. A nonzero range
   must fit completely in one of the two guest intervals. Empty spans are
   permitted through logical_end, but callers must not dereference them. */
int dhd_split_guest_range(DhdSplitLayout layout, uint64_t addr,
                          uint64_t size, uint64_t logical_end);

/* Snapshot free gaps from Linux /proc/self/maps within [floor, ceiling).
   Returns 0 on malformed maps, I/O error, bad bounds, or insufficient
   capacity. The result can immediately become stale; claim remains safe. */
int dhd_split_read_gaps(DhdAddressGap *gaps, size_t capacity, size_t *count,
                        uint64_t floor, uint64_t ceiling);

/* Claim two PROT_NONE anonymous reservations using non-destructive mmap hints;
   never MAP_FIXED. On success both have exactly the requested addresses. On
   any failure no reservation made by this call remains. Does not parse maps
   or retry. POSIX/Linux prototype, not called by the production allocator. */
int dhd_split_claim(uintptr_t base, DhdSplitLayout layout);
void dhd_split_release(uintptr_t base, DhdSplitLayout layout);
#endif
