#include "core_host.h"
#if defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
void host_fp_mode(int mode) {
    /* EE entry preserves sticky status like host_win32.c. IEEE entry clears
       status like graphics_queue.c; VU callers restore their saved raw mode. */
    _mm_setcsr(mode==HOST_FP_EE ? (_mm_getcsr() & 63u) | 0xFFC0u : 0x1F80u);
}
#elif defined(__aarch64__)
/* Untested: FPCR.RMode=11 (RZ), FZ=1, exception enables clear.
   Arm tininess/NaN details still require device state-trace validation. */
void host_fp_mode(int mode) {
    uint64_t fpcr=mode==HOST_FP_EE ? (3ull<<22)|(1ull<<24) : 0;
    __asm__ volatile("msr fpcr, %0\n\tisb" :: "r"(fpcr) : "memory");
    if (mode!=HOST_FP_EE) __asm__ volatile("msr fpsr, xzr" ::: "memory");
}
#elif defined(__arm__)
/* FPSCR carries RMode in bits 22-23 and FZ in bit 24, the same positions as FPCR: the EE mode is
   RMode=11 (RZ) with FZ, IEEE entry is 0 with the status flags (QC and the exception bits, FPSCR
   27-31 — AArch32 has no separate FPSR) cleared. Requires VFPv3 (-mfpu=neon).
   As the aarch64 path, untested on a device until a state-trace run says otherwise. */
void host_fp_mode(int mode) {
    if (mode==HOST_FP_EE) {
        uint32_t fpscr=(3u<<22)|(1u<<24);
        __asm__ volatile("vmsr fpscr, %0" :: "r"(fpscr) : "memory");
    } else {
        uint32_t fpscr;
        __asm__ volatile("vmrs %0, fpscr" : "=r"(fpscr));
        fpscr&=0x0FFFFFFFu;  /* IEEE: round to nearest, no FZ, status flags clear */
        __asm__ volatile("vmsr fpscr, %0" :: "r"(fpscr) : "memory");
    }
}
#else
#error Implement host_fp_mode for this architecture
#endif
