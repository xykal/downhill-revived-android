/* ARM NEON operand-clamp equivalence, standalone; no game data required. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "vu_neon_operand.h"

static uint32_t random32(uint32_t *s) { *s ^= *s << 13; *s ^= *s >> 17; *s ^= *s << 5; return *s; }
static uint32_t scalar(uint32_t x)
{
    uint32_t m = x & 0x7fffffffu;
    return (x & 0x80000000u) | (m > 0x7f7fffffu ? 0x7f7fffffu : m);
}
static void check(const uint32_t input[4])
{
    uint32_t out[4];
    vst1q_u32(out, vreinterpretq_u32_f32(vu_neon_operand(vreinterpretq_f32_u32(vld1q_u32(input)))));
    for (unsigned i=0; i<4; i++) if (out[i] != scalar(input[i])) {
        fprintf(stderr, "operand %08x got %08x want %08x\n", input[i],out[i],scalar(input[i])); exit(1);
    }
}
int main(void)
{
    const uint32_t corners[] = {0,0x80000000u,1,0x80000001u,0x007fffffu,0x00800000u,
        0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,0x7f800001u,0xff800001u,
        0x7fffffffu,0xffffffffu,0x3f800000u,0xbf800000u};
    for (unsigned i=0; i<sizeof(corners)/sizeof(corners[0]); i+=4) check(corners+i);
    uint32_t seed=0x35eeda81u, x[4];
    for (unsigned n=0; n<1000000; n++) { for (int i=0;i<4;i++) x[i]=random32(&seed); check(x); }
    puts("PASS: 4 million randomized ARM NEON lanes plus zero, denormal, infinities and NaNs");
    return 0;
}
