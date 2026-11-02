/* Regression test for the early guest reservation's ownership. No game data,
   actual mappings, or Android device required. Link with guest_memory.c. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "wasm-rt.h"
#include "core_host.h"
#include "guest_memory_os.h"

static unsigned reservations, closes, releases;
static int fail_next;
static uintptr_t expected_base;

uintptr_t os_reserve(void) {
    ++reservations;
    if (fail_next) { fail_next=0; return 0; }
    /* Never dereferenced: the fake OS operations below only check ownership. */
    return expected_base;
}
void os_fail(const char *what) { fprintf(stderr, "unexpected failure: %s\n", what); abort(); }
int os_split(uintptr_t at,uint64_t size) { (void)at; (void)size; return 1; }
int os_map_ram(uintptr_t at,uint64_t size,uint64_t offset) { (void)at; (void)size; (void)offset; return 1; }
int os_private(uintptr_t at,uint64_t size) { (void)at; (void)size; return 1; }
int os_commit(uintptr_t at,uint64_t size) { (void)at; (void)size; return 1; }
void os_decommit(const void *at,uint64_t size) { (void)at; (void)size; }
int os_unmap_view(void *at,uint64_t size) { (void)at; (void)size; ++releases; return 1; }
int os_release(void *at,uint64_t size) { (void)at; (void)size; ++releases; return 1; }
void os_backing_close(void) { ++closes; }
int os_fault_install(void) { return 1; }
void os_fault_remove(void) { }

static void allocate(wasm_rt_memory_t *m) {
    wasm_rt_allocate_memory(m,32768,32768,false,65536);
    assert(guest_base==expected_base && (uintptr_t)m->data==expected_base);
}
static void cleanup(wasm_rt_memory_t *m) {
    unsigned prev=closes;
    wasm_rt_free_memory(m);
    assert(!guest_base && closes==prev+1 && releases>0);
}
int main(void) {
    wasm_rt_memory_t m={0};
    expected_base=(uintptr_t)0x1000000000ull;
    guest_memory_reserve_early();
    assert(reservations==1 && guest_base==expected_base);
    guest_memory_reserve_early(); /* idempotent */
    assert(reservations==1);
    allocate(&m);
    assert(reservations==1); /* no second reservation or backing fd */
    guest_memory_reserve_early(); /* already owned */
    assert(reservations==1);
    cleanup(&m);

    fail_next=1;
    guest_memory_reserve_early();
    assert(reservations==2 && !guest_base);
    allocate(&m); /* failed early attempt is retried on instantiation */
    assert(reservations==3);
    cleanup(&m);

    allocate(&m); /* normal path without early reserve */
    assert(reservations==4);
    cleanup(&m);
    puts("PASS: early reservation reused, failed attempt retried, normal allocation and cleanup");
    return 0;
}
