/* Shared experimental split-hole range predicate. No wasm/OS dependency. */
#ifndef DHD_SPLIT_GUEST_RANGE_H
#define DHD_SPLIT_GUEST_RANGE_H
#include <stdint.h>
#include "memory_layout.h"
/* Callers enforce their own logical wasm memory size. Empty spans never
   dereference an endpoint, and are permitted by this low-level predicate. */
static inline int host64_split_span_ok(uint64_t address,uint64_t size) {
    if (!size) return 1;
    if (address<DHD_SPLIT_LOW_END) return size<=DHD_SPLIT_LOW_END-address;
    if (address>=DHD_SPLIT_HIGH_BEGIN && address<DHD_SPLIT_HIGH_END)
        return size<=DHD_SPLIT_HIGH_END-address;
    return 0;
}
/* mprotect rounds to OS pages. Only allow a nonempty range whose rounded
   pages are entirely in one guest segment. The experimental reservation is
   16-KiB aligned; reject larger or non-power-of-two host pages. */
static inline int host64_split_protect_ok(uint64_t at,uint64_t size,uint64_t page) {
    if (!size || !page || page>HOST64_PAGE || (page & (page-1)) ||
        !host64_split_span_ok(at,size)) return 0;
    uint64_t lo=at & ~(page-1);
    uint64_t hi=(at+size+page-1) & ~(page-1);
    return hi>=lo && host64_split_span_ok(lo,hi-lo);
}
#endif
