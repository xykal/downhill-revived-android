/* Tests the isolated split-reservation prototype, NOT game integration. */
#define _GNU_SOURCE
#include "split_reservation.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

static void check_plan(void) {
    /* Reconstructed from the device map (exclusive ends). */
    DhdAddressGap gaps[]={{0x05972000,0x6fc1c000},{0x759f8000,0xa2504000}};
    DhdSplitLayout l={0x3a004000,0x6fffc000,0x00e14000,0x4000};
    uint64_t base=0;
    assert(dhd_split_find(gaps,2,l,&base));
    assert(base==0x059fc000);
    assert(base+l.low_size<=gaps[0].end);
    assert(base+l.high_offset>=gaps[1].begin);
    assert(base+l.high_offset+l.high_size<=gaps[1].end);
    assert(!dhd_split_find(gaps,1,l,&base));
    assert(!dhd_split_find(gaps,2,(DhdSplitLayout){1,0,1,1},&base));
    assert(!dhd_split_find(gaps,2,(DhdSplitLayout){0x4000,0x7000,0x4000,0x3000},&base));
    printf("sample map: candidate base 0x%llx, both segments fit\n",(unsigned long long)base);
}
static void check_range(void) {
    DhdSplitLayout l={0x3a000000,0x70000000,0x00e10000,0x4000};
    uint64_t end=0x70e00000;
    assert(dhd_split_guest_range(l,0,1,end));
    assert(dhd_split_guest_range(l,0x39ffffff,1,end));
    assert(!dhd_split_guest_range(l,0x39ffffff,2,end));
    assert(!dhd_split_guest_range(l,0x3a000000,1,end));
    assert(!dhd_split_guest_range(l,0x60000000,1,end));
    assert(!dhd_split_guest_range(l,0x6fffffff,2,end));
    assert(dhd_split_guest_range(l,0x70000000,1,end));
    assert(dhd_split_guest_range(l,end-16,16,end));
    assert(!dhd_split_guest_range(l,end-16,17,end));
    assert(!dhd_split_guest_range(l,UINT64_MAX,2,end));
    assert(dhd_split_guest_range(l,0x50000000,0,end)); /* no dereference */
    assert(dhd_split_guest_range(l,end,0,end));
    assert(!dhd_split_guest_range(l,end+1,0,end));
}
static uint32_t random_word(void) {
    static uint32_t x=0x12345678;
    x^=x<<13; x^=x>>17; x^=x<<5; return x;
}
static void check_planner_against_brute_force(void) {
    for (unsigned trial=0;trial<10000;trial++) {
        DhdAddressGap gaps[4];
        for (unsigned i=0;i<4;i++) {
            uint64_t start=(random_word()%256)*16;
            gaps[i]=(DhdAddressGap){start,start+(random_word()%96+1)*16};
        }
        uint64_t low=(random_word()%16+1)*16;
        DhdSplitLayout l={low,low+(random_word()%16+1)*16,
                          (random_word()%16+1)*16,16};
        uint64_t expected=0, got=0;
        for (uint64_t b=16;b<6000;b+=16) {
            int low_ok=0, high_ok=0;
            for (unsigned i=0;i<4;i++) {
                low_ok |= b>=gaps[i].begin && b+l.low_size<=gaps[i].end;
                high_ok |= b+l.high_offset>=gaps[i].begin &&
                           b+l.high_offset+l.high_size<=gaps[i].end;
            }
            if (low_ok && high_ok) { expected=b; break; }
        }
        int found=dhd_split_find(gaps,4,l,&got);
        assert(found==(expected!=0));
        assert(!found || got==expected);
    }
    puts("PASS: split planner agrees with 10,000 brute-force layouts");
}
static void check_proc_maps(void) {
    DhdAddressGap gaps[4096]; size_t n=0;
    assert(dhd_split_read_gaps(gaps,4096,&n,0x10000,0xffff0000));
    assert(n>0);
    for (size_t i=0;i<n;i++) {
        assert(gaps[i].begin>=0x10000 && gaps[i].end<=0xffff0000);
        assert(gaps[i].begin<gaps[i].end);
        if (i) assert(gaps[i-1].end<gaps[i].begin);
    }
    /* The parser must notice a real newly occupied mapping, not report any
       part of it as free. The mmap hint is non-destructive. */
    uintptr_t address=0; long page=sysconf(_SC_PAGESIZE);
    for (size_t i=0;i<n;i++) {
        uint64_t aligned=(gaps[i].begin+(uint64_t)page-1)&~((uint64_t)page-1);
        if (aligned+(uint64_t)page<=gaps[i].end) { address=(uintptr_t)aligned; break; }
    }
    assert(address);
    void *p=mmap((void *)address,(size_t)page,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(p==(void *)address);
    assert(dhd_split_read_gaps(gaps,4096,&n,0x10000,0xffff0000));
    for (size_t i=0;i<n;i++)
        assert(gaps[i].end<=address || gaps[i].begin>=address+(uint64_t)page);
    assert(!munmap(p,(size_t)page));
    assert(!dhd_split_read_gaps(NULL,0,&n,0x10000,0xffff0000));
    assert(!dhd_split_read_gaps(gaps,4096,&n,5,5));
    puts("PASS: process-map gap snapshot excludes real occupied mapping");
}
static void check_non_destructive_claim(void) {
    long page=sysconf(_SC_PAGESIZE);
    assert(page>0);
    DhdSplitLayout l={(uint64_t)page*4,(uint64_t)page*16,(uint64_t)page*4,(uint64_t)page};
    /* Choose a fresh address by reserving a small area, then release it before
       trying the two segments. No fixed mappings anywhere in this test. */
    size_t full=(size_t)(l.high_offset+l.high_size);
    void *all=mmap(NULL,full,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS
#ifdef MAP_32BIT
                    |MAP_32BIT
#endif
                    ,-1,0);
    assert(all!=MAP_FAILED && (uint64_t)(uintptr_t)all+full<=UINT64_C(0x100000000));
    uintptr_t base=(uintptr_t)all;
    assert(!munmap(all,full));
    assert(dhd_split_claim(base,l));
    assert(!mprotect((void *)base,(size_t)page,PROT_READ|PROT_WRITE));
    assert(!mprotect((void *)(base+(uintptr_t)l.high_offset),(size_t)page,PROT_READ|PROT_WRITE));
    ((volatile unsigned char *)base)[0]=0x11;
    ((volatile unsigned char *)(base+(uintptr_t)l.high_offset))[0]=0x22;
    /* Model another library in the unowned hole; the range predicate must
       reject its address, and split cleanup must not unmap or alter it. */
    void *hole=(void *)(base+(uintptr_t)page*8);
    void *sentinel=mmap(hole,(size_t)page,PROT_READ|PROT_WRITE,
                        MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(sentinel==hole);
    ((volatile unsigned char *)sentinel)[0]=0x7b;
    assert(!dhd_split_guest_range(l,(uint64_t)page*8,1,l.high_offset+l.high_size));
    dhd_split_release(base,l);
    assert(((volatile unsigned char *)sentinel)[0]==0x7b);
    assert(!munmap(sentinel,(size_t)page));

    /* Occupy the intended high segment: claim must fail without touching it,
       and must release any low segment it temporarily acquired. */
    void *high=(void *)(base+(uintptr_t)l.high_offset);
    void *p=mmap(high,(size_t)l.high_size,PROT_READ|PROT_WRITE,
                 MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(p==high);
    ((volatile unsigned char *)p)[0]=0xa5;
    assert(!dhd_split_claim(base,l));
    assert(((volatile unsigned char *)p)[0]==0xa5);
    void *low=mmap((void *)base,(size_t)l.low_size,PROT_NONE,
                   MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(low==(void *)base); /* failed claim rolled back the first segment */
    assert(!munmap(low,(size_t)l.low_size));
    assert(!munmap(p,(size_t)l.high_size));

    /* Also verify an occupied low segment is not replaced or modified. */
    low=mmap((void *)base,(size_t)l.low_size,PROT_READ|PROT_WRITE,
             MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(low==(void *)base);
    ((volatile unsigned char *)low)[0]=0x5a;
    assert(!dhd_split_claim(base,l));
    assert(((volatile unsigned char *)low)[0]==0x5a);
    assert(!munmap(low,(size_t)l.low_size));
}
int main(void) {
    check_plan(); check_range(); check_planner_against_brute_force(); check_proc_maps(); check_non_destructive_claim();
    puts("PASS: split plan, range bounds and non-destructive reservation prototype");
    return 0;
}
