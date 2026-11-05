/* Finite clamp for VU operands, using only integer NEON operations. Preserve the sign;
   map exponent-255 bit patterns (infinity and every NaN) to signed FLT_MAX. */
#ifndef DHD_VU_NEON_OPERAND_H
#define DHD_VU_NEON_OPERAND_H
#include <arm_neon.h>
static inline __attribute__((always_inline)) float32x4_t vu_neon_operand(float32x4_t value)
{
    uint32x4_t bits = vreinterpretq_u32_f32(value);
    const uint32x4_t sign = vdupq_n_u32(0x80000000u);
    uint32x4_t magnitude = vbicq_u32(bits, sign);
    return vreinterpretq_f32_u32(vbslq_u32(sign, bits, vminq_u32(magnitude, vdupq_n_u32(0x7f7fffff))));
}
#endif
