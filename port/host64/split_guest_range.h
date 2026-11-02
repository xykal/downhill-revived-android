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
#endif
