/* Test EXPERIMENTAL wasm2c split-hole checks; NOT allocator or APK integration. */
#define DHD_EXPERIMENTAL_SPLIT_MEMORY 1
#if defined(__GNUC__) && !defined(__clang__)
/* Production uses clang; GCC is used for the ARM Linux/QEMU test binary. */
#define __builtin_assume(x) ((void)0)
#endif
#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include "memory_access.h"

static jmp_buf trap_env;
static int trap_expected;
void wasm_rt_trap(wasm_rt_trap_t trap) {
    assert(trap_expected && trap==WASM_RT_TRAP_OOB);
    longjmp(trap_env,1);
}
static uint8_t *volatile sink;
static uint8_t *fake_base(void) { return (uint8_t *)(uintptr_t)0x10000000u; }
static void run_access(unsigned kind,uint64_t addr,uint64_t len,int should_pass) {
    trap_expected=!should_pass;
    if (setjmp(trap_env)==0) {
        switch (kind) {
        case 0: sink=host64_memory_address_full(fake_base(),addr,len); break;
        case 1: sink=host64_memory_address(fake_base(),addr,len); break;
        case 2: sink=host64_access_address(fake_base(),addr,len); break;
        case 3: sink=host64_cold_address(fake_base(),addr,len); break;
        default: assert(0);
        }
        assert(should_pass);
        if (!len) assert(sink==fake_base());
    } else assert(!should_pass);
    trap_expected=0;
}
static void check_accesses(void) {
    const struct {uint64_t at,n; int valid;} cases[]={
        {0,1,1},{0x39ffffff,1,1},{0x39ffffff,2,0},
        {0x39fffff0,16,1},{0x39fffff1,16,0},
        {0x3a000000,1,0},{0x3a004000,1,0},{0x50000000,8,0},
        {0x6fffffff,1,0},{0x6fffffff,2,0},
        {0x70000000,1,1},{0x70000000,16,1},
        {0x70e0ffff,1,1},{0x70e0ffff,2,0},{0x70e10000,1,0},
        {UINT64_MAX,1,0},{0x100000000ull,16,0},
        {0x50000000,0,1},{UINT64_MAX,0,1}
    };
    for (unsigned k=0;k<4;k++)
        for (unsigned i=0;i<sizeof cases/sizeof cases[0];i++)
            run_access(k,cases[i].at,cases[i].n,cases[i].valid);
    /* Constants can select a different inline fast path. */
    assert(host64_access_address(fake_base(),0x70000000u,4)==fake_base()+0x70000000u);
    trap_expected=1;
    if (setjmp(trap_env)==0) {
        sink=host64_access_address(fake_base(),0x50000000u,4);
        assert(0);
    }
    trap_expected=0;
}
static void check_hot_path(void) {
    static const uint64_t offsets[]={0,4,0x1fffff0,0x2000000,0x39fffff0,
        0x3a000000,0x70000000,0x70e0fff0,0xffffffffull,UINT64_MAX};
    static const uint64_t edges[]={0,0x00400000,0x02000000,0x20000000,
        0x28000000,0x30000000,0x38000000,0x3a000000,0x70000000,0x70e10000,0xffffffffull};
    for (unsigned j=0;j<sizeof edges/sizeof edges[0];j++) {
        for (int delta=-32;delta<=32;delta++) {
            int64_t candidate=(int64_t)edges[j]+delta;
            if (candidate<0 || (uint64_t)candidate>UINT32_MAX) continue;
            uint32_t b=(uint32_t)candidate;
            for (unsigned k=0;k<sizeof offsets/sizeof offsets[0];k++) {
                uint64_t off=offsets[k];
                if (!host64_base_ok(b,off)) continue;
                assert(off<=UINT64_MAX-b);
                uint64_t address=(uint64_t)b+off;
                for (uint64_t n=1;n<=16;n++) {
                    assert(host64_split_span_ok(address,n));
                    assert(host64_memory_address(fake_base(),address,n)==fake_base()+address);
                }
            }
        }
    }
}
static void check_copy(void) {
    /* The forbidden src/dst must trap before memcpy/memmove can touch a host
       mapping; zero-length operations don't touch either endpoint. */
    host64_memory_copy(fake_base(),UINT64_MAX,fake_base(),UINT64_MAX,0);
    for (volatile unsigned i=0;i<2;i++) {
        trap_expected=1;
        if (setjmp(trap_env)==0) {
            host64_memory_copy(fake_base(),i?0x50000000u:0x08000000u,
                               fake_base(),i?0x08000000u:0x50000000u,4);
            assert(0);
        }
        trap_expected=0;
    }
}
int main(void) {
    check_accesses(); check_hot_path(); check_copy();
    puts("PASS: split wasm address guards, fast/cold paths, boundaries, copy");
    return 0;
}
