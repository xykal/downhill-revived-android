/* Android fatal-signal logging. Metadata is collected before signals; the handler only
   formats bounded local data and writes directly to the already-open append log. Never
   probe the interrupted stack or call malloc, stdio, dladdr or a lock in this path. */
#ifndef DHD_ANDROID_CRASH_LOG_H
#define DHD_ANDROID_CRASH_LOG_H
#include <link.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/ucontext.h>

struct dhd_crash_image {
    uintptr_t base, lo, hi;
    char build_id[129];
};
static struct dhd_crash_image dhd_image;
static int dhd_crash_fd = -1;

static int dhd_image_collect(struct dl_phdr_info *info, size_t size, void *data)
{
    (void)size;
    uintptr_t needle = (uintptr_t)data;
#if defined(__arm__)
    needle &= ~(uintptr_t)1;
#endif
    for (unsigned i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr) *p = &info->dlpi_phdr[i];
        uintptr_t lo = (uintptr_t)info->dlpi_addr + p->p_vaddr;
        if (p->p_type != PT_LOAD || !(p->p_flags & PF_X) || needle < lo || needle - lo >= p->p_memsz) continue;
        dhd_image.base = info->dlpi_addr;
        dhd_image.lo = lo;
        dhd_image.hi = lo + p->p_memsz;
        /* PT_NOTE is mapped with the image on Android; use only notes contained in a
           readable PT_LOAD. Read and decode here, not in the signal handler. */
        for (unsigned j = 0; j < info->dlpi_phnum; ++j) {
            const ElfW(Phdr) *n = &info->dlpi_phdr[j];
            if (n->p_type != PT_NOTE || n->p_memsz < 12) continue;
            int mapped = 0;
            for (unsigned k = 0; k < info->dlpi_phnum; ++k) {
                const ElfW(Phdr) *l = &info->dlpi_phdr[k];
                if (l->p_type == PT_LOAD && (l->p_flags & PF_R) && n->p_vaddr >= l->p_vaddr &&
                    n->p_vaddr - l->p_vaddr <= l->p_memsz && n->p_memsz <= l->p_memsz - (n->p_vaddr - l->p_vaddr)) mapped = 1;
            }
            if (!mapped) continue;
            const unsigned char *b = (const unsigned char *)((uintptr_t)info->dlpi_addr + n->p_vaddr);
            size_t at = 0, total = n->p_memsz;
            while (total - at >= 12) {
                uint32_t namesz, descsz, type;
                memcpy(&namesz, b + at, 4); memcpy(&descsz, b + at + 4, 4); memcpy(&type, b + at + 8, 4);
                at += 12;
                size_t na = ((size_t)namesz + 3u) & ~(size_t)3u, da = ((size_t)descsz + 3u) & ~(size_t)3u;
                if (na < namesz || da < descsz || na > total - at || da > total - at - na) break;
                if (type == 3 && namesz == 4 && !memcmp(b + at, "GNU\0", 4) && descsz > 0 && descsz <= 64) {
                    static const char hex[] = "0123456789abcdef";
                    for (unsigned x = 0; x < descsz; ++x) {
                        unsigned v = b[at + na + x];
                        dhd_image.build_id[x * 2] = hex[v >> 4]; dhd_image.build_id[x * 2 + 1] = hex[v & 15];
                    }
                    dhd_image.build_id[descsz * 2] = 0;
                }
                at += na + da;
            }
        }
        return 1;
    }
    return 0;
}
static void dhd_crash_init(int fd, const void *code_address)
{
    dhd_crash_fd = fd;
    dl_iterate_phdr(dhd_image_collect, (void *)code_address);
}
struct dhd_crash_buf { char text[2048]; size_t n; };
static void dhd_cstr(struct dhd_crash_buf *b, const char *s)
{
    while (*s && b->n < sizeof b->text) b->text[b->n++] = *s++;
}
static void dhd_hex(struct dhd_crash_buf *b, uintptr_t n)
{
    static const char h[] = "0123456789abcdef";
    dhd_cstr(b, "0x");
    for (int i = (int)(sizeof n * 2) - 1; i >= 0; --i)
        if (b->n < sizeof b->text) b->text[b->n++] = h[(n >> (i * 4)) & 15];
}
static void dhd_val(struct dhd_crash_buf *b, const char *key, uintptr_t n)
{
    dhd_cstr(b, key); dhd_hex(b, n); dhd_cstr(b, " ");
}
static void dhd_relative(struct dhd_crash_buf *b, const char *name, uintptr_t pc)
{
    uintptr_t addr = pc;
#if defined(__arm__)
    addr &= ~(uintptr_t)1; /* LR bit zero records Thumb state, not an address bit. */
#endif
    dhd_cstr(b, name);
    if (dhd_image.hi > dhd_image.lo && addr >= dhd_image.lo && addr < dhd_image.hi) {
        dhd_cstr(b, "libmain+"); dhd_hex(b, addr - dhd_image.base);
    } else dhd_cstr(b, "outside-libmain-or-unknown");
    dhd_cstr(b, "\n");
}
static void dhd_crash_write(int sig, const siginfo_t *info, const ucontext_t *ctx, uintptr_t guest)
{
    struct dhd_crash_buf b = {{0}, 0};
    uintptr_t addr = info ? (uintptr_t)info->si_addr : 0;
    uintptr_t pc = 0, lr = 0;
    dhd_cstr(&b, "\n=== FATAL SIGNAL (direct crash record) ===\n");
    dhd_val(&b, "signal=", (uintptr_t)sig);
    dhd_val(&b, "si_code=", (uintptr_t)(intptr_t)(info ? info->si_code : 0));
    dhd_val(&b, "fault=", addr); dhd_val(&b, "guest_base=", guest);
    if (guest && addr >= guest) dhd_val(&b, "guest_offset_if_in_range=", addr - guest);
    dhd_cstr(&b, "\n");
#if defined(__arm__)
    if (ctx) {
        const mcontext_t *r = &ctx->uc_mcontext;
        pc = r->arm_pc; lr = r->arm_lr;
        dhd_val(&b,"pc=",pc); dhd_val(&b,"lr=",lr); dhd_val(&b,"sp=",r->arm_sp); dhd_val(&b,"cpsr=",r->arm_cpsr); dhd_cstr(&b,"\n");
        dhd_val(&b,"r0=",r->arm_r0); dhd_val(&b,"r1=",r->arm_r1); dhd_val(&b,"r2=",r->arm_r2); dhd_val(&b,"r3=",r->arm_r3); dhd_cstr(&b,"\n");
        dhd_val(&b,"r4=",r->arm_r4); dhd_val(&b,"r5=",r->arm_r5); dhd_val(&b,"r6=",r->arm_r6); dhd_val(&b,"r7=",r->arm_r7); dhd_cstr(&b,"\n");
        dhd_val(&b,"r8=",r->arm_r8); dhd_val(&b,"r9=",r->arm_r9); dhd_val(&b,"r10=",r->arm_r10); dhd_val(&b,"r11=",r->arm_fp); dhd_val(&b,"r12=",r->arm_ip); dhd_cstr(&b,"\n");
    }
#elif defined(__x86_64__) && defined(DHD_CRASH_TEST)
    if (ctx) { pc = ctx->uc_mcontext.gregs[REG_RIP]; dhd_val(&b,"pc=",pc); dhd_cstr(&b,"\n"); }
#endif
    dhd_val(&b, "libmain_load_bias=", dhd_image.base); dhd_cstr(&b, "build_id=");
    dhd_cstr(&b, dhd_image.build_id[0] ? dhd_image.build_id : "unavailable"); dhd_cstr(&b,"\n");
    dhd_relative(&b, "pc_image=", pc); dhd_relative(&b, "lr_image=", lr);
    dhd_cstr(&b, "=== END FATAL SIGNAL ===\n");
    int fd = dhd_crash_fd >= 0 ? dhd_crash_fd : STDERR_FILENO;
    size_t at = 0;
    while (at < b.n) {
        ssize_t n = write(fd, b.text + at, b.n - at);
        if (n > 0) at += (size_t)n;
        else if (n < 0 && errno == EINTR) continue;
        else break;
    }
}
#endif
