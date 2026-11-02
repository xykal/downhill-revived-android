/* Real POSIX-mmap integration test of the existing contiguous allocator.
   No game data. Intended for Linux x64 and emulated 32-bit ARM Linux; it is
   not an Android device test and does not test the experimental split code. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "wasm-rt.h"
#include "core_host.h"

int main(void) {
    wasm_rt_memory_t m={0};
    guest_memory_reserve_early();
    if (!guest_base) {
        fputs("SKIP: host cannot fit contiguous guest reservation\n",stderr);
        return 77;
    }
    uintptr_t first=guest_base;
    wasm_rt_allocate_memory(&m,0x70e0,0x70e2,false,65536);
    assert(guest_base==first && (uintptr_t)m.data==first);
    volatile uint32_t *module=(volatile uint32_t *)(uintptr_t)(first+0x70100000u);
    *module=0xabcdef01u;
    assert(*module==0xabcdef01u);
    assert(host64_memory_commit(0x70000000,0x4000));
    volatile uint32_t *scratch=(volatile uint32_t *)(uintptr_t)(first+0x70000000u);
    *scratch=0x12345678u;
    assert(*scratch==0x12345678u);
    volatile uint32_t *ram=(volatile uint32_t *)(uintptr_t)(first+0x08000100u);
    volatile uint32_t *mirror=(volatile uint32_t *)(uintptr_t)(first+0x38000100u);
    *ram=0x5138abcdu;
    assert(*mirror==0x5138abcdu);
    *mirror=0xa1b2c3d4u;
    assert(*ram==0xa1b2c3d4u);
    assert(wasm_rt_grow_memory(&m,1)==0x70e0);
    volatile uint32_t *grown=(volatile uint32_t *)(uintptr_t)(first+0x70e00000u);
    *grown=7;
    assert(*grown==7);
#if UINTPTR_MAX == 0xffffffffu
    assert(wasm_rt_grow_memory(&m,1)==UINT32_MAX && m.pages==0x70e1);
#else
    assert(wasm_rt_grow_memory(&m,1)==0x70e1);
#endif
    wasm_rt_free_memory(&m);
    assert(!guest_base);
    puts("PASS: real mmap, early reuse, module/scratch commits, mirrors, growth, release");
    return 0;
}
