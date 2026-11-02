/* Experimental actual wasm2c execution against the port allocator configured
   for split mappings. This is not enabled in the Android build or release. */
#define _GNU_SOURCE
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "wasm-rt.h"
#include "wasm-rt-impl.h"
#include "core_host.h"
#include "bridge.h"
#include "../include/guest.h"
#include "core.h"

/* The full application provides this; the isolated fixture has no log mirror. */
void host_log_drain(void) {}
static uintptr_t base;
static w2c_core core;
static int bad_fault_callback(uint32_t address) {
    (void)address;
    _exit(42); /* The unrelated hole must never be offered to the guest. */
}
static void check_unrelated_fault(void *sentinel) {
    pid_t child=fork();
    assert(child>=0);
    if (!child) {
        struct rlimit no_core={0,0};
        (void)setrlimit(RLIMIT_CORE,&no_core);
        assert(host_fault_guard(bad_fault_callback));
        assert(!mprotect(sentinel,0x4000,PROT_NONE));
        volatile uint32_t word=*(volatile uint32_t *)sentinel;
        (void)word;
        _exit(0);
    }
    int status=0;
    assert(waitpid(child,&status,0)==child);
    assert(WIFSIGNALED(status) && WTERMSIG(status)==SIGSEGV);
}
static void check_native_guard(void) {
    assert(GUEST(0x08000000u)==(void *)(base+0x08000000u));
    pid_t child=fork();
    assert(child>=0);
    if (!child) {
        struct rlimit no_core={0,0};
        (void)setrlimit(RLIMIT_CORE,&no_core);
        (void)GUEST(0x50000000u);
        _exit(0);
    }
    int status=0;
    assert(waitpid(child,&status,0)==child);
    assert(WIFSIGNALED(status) && WTERMSIG(status)==SIGABRT);
}
static void expect_trap(unsigned operation) {
    wasm_rt_trap_t trap=WASM_RT_SETJMP(g_wasm_rt_jmp_buf);
    if (trap==WASM_RT_TRAP_NONE) {
        switch (operation) {
        case 0: (void)w2c_core_load(&core,0x50000000); break;
        case 1: w2c_core_store(&core,0x50000000,0x11223344); break;
        case 2: (void)w2c_core_load(&core,0x39fffffe); break;
        case 3: w2c_core_fill(&core,0x50000000,0x99,4); break;
        case 4: w2c_core_copy(&core,0x50000000,0x08000000,4); break;
        case 5: w2c_core_copy(&core,0x08000000,0x50000000,4); break;
        case 6: (void)w2c_core_load_offset(&core,0x10000000); break;
        case 7: (void)host64_bridge_pointer(0x50000000,4); break;
        case 8: (void)host64_bridge_pointer(0x39fffffe,4); break;
        case 9: (void)host64_bridge_pointer(0x50000000,0); break;
        case 10: (void)host64_bridge_string(0x39fffffc); break;
        default: assert(0);
        }
        assert(!"expected trap");
    }
    assert(trap==WASM_RT_TRAP_OOB);
}
int main(void) {
    wasm_rt_init();
    guest_memory_reserve_early();
    assert(guest_base);
    base=guest_base;
    /* Place real unrelated writable host memory inside the unmapped guest
       hole. A buggy raw base+guest access would corrupt it, not merely fault. */
    void *target=(void *)(base+0x50000000u);
    void *sentinel=mmap(target,0x4000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if (sentinel!=target) {
        if (sentinel!=MAP_FAILED) munmap(sentinel,0x4000);
        fputs("unable to place non-destructive hole sentinel\n",stderr); return 1;
    }
    *(volatile uint32_t *)sentinel=0xa55aa55a;
    wasm2c_core_instantiate(&core);
    assert(base==guest_base);
    host64_bridge_bind(w2c_core_memory(&core));
    assert(host_mem_reserved(0x08000000,4));
    assert(!host_mem_reserved(0x50000000,4));
    assert(!host_mem_reserved(0x39fffffe,4));
    assert(host64_memory_commit(0x70000000u,0x4000));
    assert(!host64_memory_commit(0x3a000000u,0x4000));
    assert(!host64_memory_commit(0x50000000u,0x4000));
    assert(!host64_memory_commit(0x39fff000u,0x2000));
    assert(*(volatile uint32_t *)sentinel==0xa55aa55a);
    assert(!host_mem_protect(0x50000000u,4,HOST_MEM_NOACCESS));
    assert(!host_mem_protect(0x39fffffeu,4,HOST_MEM_NOACCESS));
    assert(!host_mem_protect(0x08000000u,0x48000004u,HOST_MEM_NOACCESS));
    assert(host_mem_protect(0x08000000u,0x4000,HOST_MEM_NOACCESS));
    assert(host_mem_protect(0x08000000u,0x4000,HOST_MEM_READWRITE));
    assert(*(volatile uint32_t *)sentinel==0xa55aa55a);
    check_native_guard();
    check_unrelated_fault(sentinel);
    w2c_core_store(&core,0x08000000,0x12345678);
    assert(w2c_core_load(&core,0x08000000)==0x12345678);
    assert(w2c_core_load(&core,0x38000000)==0x12345678);
    assert(*(volatile uint32_t *)(uintptr_t)(base+0x38000000u)==0x12345678);
    w2c_core_store(&core,0x70000000,0x87654321);
    assert(w2c_core_load(&core,0x70000000)==0x87654321);
    assert(w2c_core_load_offset(&core,0)==0x87654321);
    w2c_core_fill(&core,0x08000004,0x5a,4);
    assert(w2c_core_load(&core,0x08000004)==0x5a5a5a5a);
    w2c_core_copy(&core,0x08000008,0x08000000,4);
    assert(w2c_core_load(&core,0x08000008)==0x12345678);
    /* Empty spans in the hole are legal after wasm bounds checks. */
    w2c_core_fill(&core,0x50000000,0x99,0);
    w2c_core_copy(&core,0x50000000,0x50000000,0);
    memcpy((void *)(base+0x08000010u),"hello",6);
    assert(!strcmp(host64_bridge_string(0x08000010),"hello"));
    memset((void *)(base+0x39fffffcu),'X',4); /* unterminated at the hole boundary */
    for (unsigned i=0;i<11;i++) expect_trap(i);
    assert(*(volatile uint32_t *)sentinel==0xa55aa55a);
    wasm_rt_memory_t *m=w2c_core_memory(&core);
    assert(wasm_rt_grow_memory(m,1)==0x70e0);
    assert(wasm_rt_grow_memory(m,1)==UINT32_MAX);
    wasm2c_core_free(&core);
    assert(*(volatile uint32_t *)sentinel==0xa55aa55a);
    assert(!munmap(sentinel,0x4000));
    wasm_rt_free();
    puts("PASS: real split mmap + generated wasm2c guards preserve hole sentinel");
    return 0;
}
