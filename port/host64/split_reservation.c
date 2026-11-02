/* Segmented guest-reservation proof of concept. Not used by the game yet. */
#define _GNU_SOURCE
#include "split_reservation.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#define LIMIT32 (UINT64_C(1) << 32)
static int valid_layout(DhdSplitLayout l) {
    return l.alignment && !(l.alignment & (l.alignment-1)) &&
           l.low_size && l.high_size &&
           !(l.low_size % l.alignment) && !(l.high_offset % l.alignment) &&
           !(l.high_size % l.alignment) && l.low_size <= l.high_offset &&
           l.high_offset <= LIMIT32 && l.high_size <= LIMIT32-l.high_offset;
}
int dhd_split_find(const DhdAddressGap *gaps, size_t n, DhdSplitLayout l,
                   uint64_t *base) {
    if (!base || (!gaps && n) || !valid_layout(l)) return 0;
    uint64_t best=LIMIT32;
    for (size_t i=0;i<n;i++) {
        uint64_t lo=gaps[i].begin, end=gaps[i].end;
        if (lo>=end || end>LIMIT32 || end-lo<l.low_size) continue;
        uint64_t low_max=end-l.low_size;
        for (size_t j=0;j<n;j++) {
            uint64_t hs=gaps[j].begin, he=gaps[j].end;
            if (hs>=he || he>LIMIT32 || he-hs<l.high_size ||
                he<l.high_offset+l.high_size) continue;
            uint64_t high_min=hs>l.high_offset ? hs-l.high_offset : 0;
            uint64_t high_max=he-l.high_offset-l.high_size;
            uint64_t min=lo>high_min ? lo : high_min;
            uint64_t max=low_max<high_max ? low_max : high_max;
            /* A null base signals failure; try the first nonzero aligned base
               instead of rejecting an otherwise usable gap that starts at 0. */
            if (min<l.alignment) min=l.alignment;
            /* At most 32-bit inputs; do this in 64 bits before alignment. */
            uint64_t aligned=(min+l.alignment-1)&~(l.alignment-1);
            if (aligned && aligned<=max && aligned<best) best=aligned;
        }
    }
    if (best==LIMIT32) return 0;
    *base=best;
    return 1;
}
int dhd_split_guest_range(DhdSplitLayout l, uint64_t addr,
                          uint64_t size, uint64_t logical_end) {
    if (!valid_layout(l) || addr>logical_end || size>logical_end-addr) return 0;
    if (!size) return 1;
    if (addr<l.low_size && size<=l.low_size-addr) return 1;
    if (addr>=l.high_offset && addr<l.high_offset+l.high_size &&
        size<=l.high_offset+l.high_size-addr) return 1;
    return 0;
}
int dhd_split_read_gaps(DhdAddressGap *gaps, size_t capacity, size_t *count,
                        uint64_t floor, uint64_t ceiling) {
    if (!count || (!gaps && capacity) || floor>=ceiling || ceiling>LIMIT32) return 0;
    *count=0;
    FILE *maps=fopen("/proc/self/maps","r");
    if (!maps) return 0;
    char line[512]; uint64_t cursor=floor, previous_end=0;
    size_t n=0; int ok=1;
    while (fgets(line,sizeof line,maps)) {
        unsigned long long a,b;
        if (sscanf(line,"%llx-%llx",&a,&b)!=2 || a>=b || a<previous_end) {
            ok=0; break;
        }
        previous_end=b;
        /* Long path names can overflow the line buffer; discard the rest so
           a continuation is never mistaken for a new map line. */
        if (!strchr(line,'\n')) {
            int ch;
            while ((ch=fgetc(maps))!=EOF && ch!='\n') { }
        }
        if (a>=ceiling) break;
        if (b<=floor) continue;
        uint64_t lo=a<floor ? floor : (uint64_t)a;
        uint64_t hi=b>ceiling ? ceiling : (uint64_t)b;
        if (lo>cursor) {
            if (n==capacity) { ok=0; break; }
            gaps[n++]=(DhdAddressGap){cursor,lo};
        }
        if (hi>cursor) cursor=hi;
    }
    if (ferror(maps)) ok=0;
    if (ok && cursor<ceiling) {
        if (n==capacity) ok=0;
        else gaps[n++]=(DhdAddressGap){cursor,ceiling};
    }
    if (fclose(maps)) ok=0;
    if (!ok) return 0;
    *count=n;
    return 1;
}
int dhd_split_claim(uintptr_t base, DhdSplitLayout l) {
    if (!base || !valid_layout(l) || (uint64_t)base%l.alignment ||
        (uint64_t)base>LIMIT32-l.high_offset-l.high_size ||
        l.low_size>SIZE_MAX || l.high_size>SIZE_MAX) return 0;
    void *low=(void *)base;
    void *high=(void *)(uintptr_t)((uint64_t)base+l.high_offset);
    void *p=mmap(low,(size_t)l.low_size,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if (p==MAP_FAILED) return 0;
    if (p!=low) { munmap(p,(size_t)l.low_size); return 0; }
    p=mmap(high,(size_t)l.high_size,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if (p==MAP_FAILED) { munmap(low,(size_t)l.low_size); return 0; }
    if (p!=high) {
        munmap(p,(size_t)l.high_size); munmap(low,(size_t)l.low_size); return 0;
    }
    return 1;
}
void dhd_split_release(uintptr_t base, DhdSplitLayout l) {
    munmap((void *)base,(size_t)l.low_size);
    munmap((void *)(uintptr_t)((uint64_t)base+l.high_offset),(size_t)l.high_size);
}
