#define _GNU_SOURCE
#define DHD_CRASH_TEST 1
#include "android_crash_log.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>

static volatile unsigned char *base;
static void crash_handler(int sig, siginfo_t *info, void *ctx)
{
    dhd_crash_write(sig, info, ctx, (uintptr_t)base);
    if (info && info->si_code > 0) return;
    _exit(128 + sig);
}
__attribute__((noinline)) static void fault_at_guest_20(void)
{
    base[0x20] = 7;
}
int main(void)
{
    char path[] = "/tmp/dhd-crash-test-XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) return 1;
    base = mmap(NULL, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) return 2;
    dhd_crash_init(fd, (void *)&main);
    if (!dhd_image.hi || !dhd_image.build_id[0]) return 3;
    pid_t child = fork();
    if (child == 0) {
        struct sigaction sa = {0};
        sa.sa_sigaction = crash_handler;
        sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGSEGV, &sa, NULL);
        fault_at_guest_20();
        _exit(88);
    }
    int status;
    if (child < 0 || waitpid(child, &status, 0) < 0) return 4;
    if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGSEGV) return 5;
    char text[4096] = {0};
    lseek(fd, 0, SEEK_SET);
    ssize_t n = read(fd, text, sizeof text - 1);
    close(fd); unlink(path);
    const char *offset = strstr(text, "guest_offset_if_in_range=");
    if (n <= 0 || !offset || strtoull(offset + strlen("guest_offset_if_in_range="), NULL, 16) != 0x20 ||
        !strstr(text, "pc_image=libmain+0x") || !strstr(text, "=== END FATAL SIGNAL ===") ||
        strstr(text, "build_id=unavailable")) return 6;
    printf("crash regression: passed (real SIGSEGV, guest+0x20, PC, build ID, original signal)\n%s", text);
    return 0;
}
