/* fiber.c's switch for AAPCS32 (Android armeabi-v7a): r4-r11 are callee-saved, and lr holds the
   return address; d8-d15 (the low halves the AAPCS32 VFP supplementary standard register save
   requires) and FPSCR complete the frame. As fiber_arm64.c, but 32-bit slots; the frame stays
   8-byte aligned (AAPCS32's public-interface alignment, and vstmia's). Thumb-2 and ARM state
   both assemble these instructions; the NDK compiles armeabi-v7a as Thumb. */
#include <stdint.h>
#include <string.h>
#include "fiber_switch.h"

#ifdef __APPLE__
#error No 32-bit Apple target; this adapter is Android/Linux armv7 only
#endif
#define ASM_FUNCTION(name) ".globl " #name "\n.type " #name ",%function\n.p2align 2\n" #name ":\n"
#define ASM_SYMBOL(name) #name

/* r0=&save, r1=resume. One 112-byte frame:
     [sp+0]   r4 r5    [sp+8]   r6 r7    [sp+16]  r8 r9    [sp+24]  r10 r11
     [sp+32]  lr (the return address; sp saved beside it)
     [sp+40]  d8 d9    [sp+56]  d10 d11  [sp+72]  d12 d13  [sp+88]  d14 d15
     [sp+104] FPSCR
   A new fiber's frame "returns" (bx lr) into fiber_start with r4 holding fiber_entry's argument. */
__asm__(
".text\n"
".syntax unified\n"
ASM_FUNCTION(fiber_switch_asm)
"  sub sp, sp, #112\n"
"  stmia sp, {r4-r11}\n"
"  str lr, [sp, #32]\n"
"  add r2, sp, #112\n"
"  str r2, [sp, #36]\n"
"  vstmia sp, {d8-d15}\n"
"  vmrs r2, fpscr\n"
"  str r2, [sp, #104]\n"
"  str sp, [r0]\n"
"  mov sp, r1\n"
"  ldmia sp, {r4-r11}\n"
"  ldr lr, [sp, #32]\n"
"  vldmia sp, {d8-d15}\n"
"  ldr r2, [sp, #104]\n"
"  vmsr fpscr, r2\n"
"  add sp, sp, #112\n"
"  bx lr\n"
ASM_FUNCTION(fiber_start)
"  mov r0, r4\n"
"  bl " ASM_SYMBOL(fiber_entry) "\n"
"  udf #0\n"
);
void fiber_start(void);

void *fiber_frame(uintptr_t lo,uintptr_t hi,void *arg) {
    (void)lo;
    /* r11 = 0 ends the frame chain; sp stays 8-aligned before and after the frame. */
    uint32_t *frame=(uint32_t *)(((hi-8)&~(uintptr_t)7)-112), fpscr;
    __asm__ volatile("vmrs %0, fpscr" : "=r"(fpscr));
    memset(frame,0,112);
    frame[0]=(uint32_t)(uintptr_t)arg;      /* r4: fiber_start moves it to r0 */
    frame[8]=(uint32_t)(uintptr_t)fiber_start; /* lr */
    frame[26]=fpscr;
    return frame;
}
