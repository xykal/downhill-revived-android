/* The Android start (finding 091). SDL's Java activity (port/android: DhdActivity over SDL's
   SDLActivity) loads libSDL3.so and libmain.so, then calls SDL_main here on its SDL thread. That
   thread becomes SDL's loop (port/host/sdl_host.c) and the game gets a thread of its own, as on
   every platform, once the game's own main runs (port/host64/launcher.c, compiled as
   dhd_launcher_main). Before it:
   - the log (stdout, stderr) goes to logcat (tag DownhillRevived) and to dhd.log (host.c's
     host_console_init, the log of every platform, started here before anything prints);
   - the app's files folder (internal storage, SDL_GetAndroidInternalStoragePath) stands for the
     folder of dhd.exe: host_exe_path names dhd64 there, so dhd.ini, saves, dhd.log and the disc
     image's copy live there;
   - the disc image: the .iso the player picks once through Android's document picker (SDL's
     file dialog). It is played where it is through the content URI Android hands back (no copy:
     the files folder and internal storage stay as they are); a copy into SCUS-97177.iso in that
     folder only when the file can't be read in place;
   - a new dhd.ini names the copy (iso) and turns the updater off (an APK is updated by reinstalling
     it); the player defaults (the native renderer at 2x, widescreen, movies skipped) come from
     host.c as on Windows;
   - SDL hints: landscape only, the Back button kept from closing the game.
   The game ends with exit() (the close of the activity, gs_pump_messages, on the game's thread);
   SDL_main never returns to a second start in the same process. */
#include <SDL3/SDL.h>
#include <android/log.h>
#include <jni.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../render/include/dhd_settings.h"

#define TAG "DownhillRevived"
#define DISC_NAME "SCUS-97177.iso"

int dhd_launcher_main(int argc, char **argv);   /* launcher.c's main */
void host_console_init(void);                   /* host.c: the log */
const char *host_disc_check(const char *path);  /* host.c: NULL when path is the game */

static char files[1024];      /* the app's files folder, with a trailing slash */
static char exe[1100];        /* files + "dhd64": what host_exe_path answers */
static Uint64 boot_start_ms;
static atomic_int settings_ready;
int android_settings_ready(void) { return atomic_load_explicit(&settings_ready, memory_order_acquire); }
#ifndef DHD_BUILD_ID
#define DHD_BUILD_ID "unknown"
#endif

/* Progress goes to the same dhd.log and logcat, once per phase, not each frame.
   Times are relative to SDL_main, not claims about actual screen visibility. */
void android_boot_note(const char *phase)
{
    Uint64 now = SDL_GetTicks();
    fprintf(stderr, "[BOOT +%llu.%03llus] %s\n",
            (unsigned long long)((now - boot_start_ms) / 1000),
            (unsigned long long)((now - boot_start_ms) % 1000), phase);
    fflush(stderr);
}

/* host_posix.c's host_exe_path on Android. */
const char *android_exe_path(void) { return exe; }

/* ---- the disc image ---- */

static SDL_Semaphore *picked;
static char chosen[4096];

static void SDLCALL on_pick(void *userdata, const char *const *list, int filter)
{
    (void)userdata; (void)filter;
    chosen[0] = 0;
    if (list && list[0]) snprintf(chosen, sizeof chosen, "%s", list[0]);
    SDL_SignalSemaphore(picked);
}

/* The document the player picks (a content:// URI), or "" when cancelled. */
static const char *pick(void)
{
    if (!picked) picked = SDL_CreateSemaphore(0);
    SDL_ShowOpenFileDialog(on_pick, NULL, NULL, NULL, 0, NULL, false);  /* no filter: any file */
    SDL_WaitSemaphore(picked);
    return chosen;
}

/* Copies the document at uri to path (through a .part file); 1 when done. */
static int copy_disc(const char *uri, const char *path)
{
    char part[1300];
    snprintf(part, sizeof part, "%s.part", path);
    SDL_IOStream *in = SDL_IOFromFile(uri, "rb");
    if (!in) { fprintf(stderr, "android: %s can't be opened (%s)\n", uri, SDL_GetError()); return 0; }
    FILE *out = fopen(part, "wb");
    if (!out) { SDL_CloseIO(in); fprintf(stderr, "android: %s can't be written\n", part); return 0; }
    static char block[1 << 20];
    uint64_t total = 0, next = 0;
    size_t n;
    int ok = 1;
    while ((n = SDL_ReadIO(in, block, sizeof block)) > 0) {
        if (fwrite(block, 1, n, out) != n) { ok = 0; break; }
        total += n;
        if (total >= next) { fprintf(stderr, "android: copying the disc image, %llu MB\n", (unsigned long long)(total >> 20)); next += 256u << 20; }
    }
    if (SDL_GetIOStatus(in) == SDL_IO_STATUS_ERROR) ok = 0;
    SDL_CloseIO(in);
    ok &= fclose(out) == 0;
    if (ok && total && rename(part, path) == 0) {
        fprintf(stderr, "android: disc image copied, %llu MB\n", (unsigned long long)(total >> 20));
        return 1;
    }
    unlink(part);
    return 0;
}

/* ---- keeping the pick across restarts ---- */
/* SDL's document picker grants the read for this session only; keep it (Android allows that for
   picked documents) so the next start doesn't have to choose the disc again. Best effort: when
   the grant wasn't persistable the next start simply asks again. */
static void keep_permission(const char *uri)
{
    JNIEnv *env = SDL_GetAndroidJNIEnv();  /* void* here: C converts it to JNIEnv* */
    jobject activity = SDL_GetAndroidActivity();
    jclass uri_cls, act_cls, cr_cls;
    jmethodID parse, get_cr, take;
    jstring juri;
    jobject parsed, cr;
    if (!env || !activity) return;
    uri_cls = (*env)->FindClass(env, "android/net/Uri");
    if (!uri_cls) goto done;
    parse = (*env)->GetStaticMethodID(env, uri_cls, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
    act_cls = (*env)->FindClass(env, "android/app/Activity");
    if (!act_cls) goto done;
    get_cr = (*env)->GetMethodID(env, act_cls, "getContentResolver", "()Landroid/content/ContentResolver;");
    cr_cls = (*env)->FindClass(env, "android/content/ContentResolver");
    if (!cr_cls) goto done;
    take = (*env)->GetMethodID(env, cr_cls, "takePersistableUriPermission", "(Landroid/net/Uri;I)V");
    if (!parse || !get_cr || !take) goto done;
    juri = (*env)->NewStringUTF(env, uri);
    parsed = juri ? (*env)->CallStaticObjectMethod(env, uri_cls, parse, juri) : NULL;
    cr = parsed ? (*env)->CallObjectMethod(env, activity, get_cr) : NULL;
    if (cr) {
        (*env)->CallVoidMethod(env, cr, take, parsed, 1 /* FLAG_GRANT_READ_URI_PERMISSION */);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
            fprintf(stderr, "android: the disc's access can't be kept; the game will ask for it "
                    "again after it restarts (%s)\n", uri);
        }
    }
done:
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
}

/* ---- the crash report ---- */
/* A force close is otherwise only in logcat: write the crash into dhd.log (stderr goes there,
   and to logcat) with the signal and a backtrace — llvm-addr2line -e libmain.so turns the
   frames into source lines — then let the default action end the process. Installed before the
   game's fault guard (host_posix.c), which chains to it for faults that aren't its own. */
extern void host_backtrace(void);

static void crash_report(int sig)
{
    fprintf(stderr, "\n========== [CRASH] signal %d ==========\n", sig);
    fprintf(stderr, "[CRASH] See the last [BOOT] phase and ERROR lines above; backtrace follows.\n");
    host_backtrace();
    fprintf(stderr, "========== [END CRASH] ==========\n");
    fflush(NULL);
    signal(sig, SIG_DFL);
    raise(sig);
}

static void install_crash_report(void)
{
    static const int sigs[] = {SIGSEGV, SIGBUS, SIGABRT, SIGILL, SIGFPE};
    for (size_t i = 0; i < sizeof sigs / sizeof sigs[0]; i++) signal(sigs[i], crash_report);
}

/* The disc image: what dhd.ini already names when it still reads and is the game — a picked
   content URI plays in place, a copy in the files folder as it always did — else the picker.
   iso_out gets what the game should be given (settings_start writes it to dhd.ini). 0 when
   the player gave up. */
static int disc_ready(char *iso_out, size_t iso_size)
{
    struct stat st;
    char copy[1200], ini[1200], have[4096];
    snprintf(copy, sizeof copy, "%s" DISC_NAME, files);
    snprintf(ini, sizeof ini, "%sdhd.ini", files);
    if (dhd_settings_read(ini, "iso", have, sizeof have) && have[0] && !host_disc_check(have)) {
        snprintf(iso_out, iso_size, "%s", have);
        return 1;
    }
    if (stat(copy, &st) == 0 && !host_disc_check(copy)) {  /* a copy from an older start */
        snprintf(iso_out, iso_size, "%s", copy);
        return 1;
    }
    char message[1024];
    snprintf(message, sizeof message, "Choose your Downhill Domination (USA) disc image: the game "
             "plays from your own copy of the disc, an .iso file (SCUS-97177), read in place where "
             "you keep it — no copy and no extra storage needed.");
    for (;;) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Downhill Revived", message, NULL);
        const char *uri = pick();
        if (!*uri) return 0;
        fprintf(stderr, "android: disc image chosen: %s\n", uri);
        const char *why = host_disc_check(uri);
        if (!why) {
            keep_permission(uri);  /* it stays chosen across restarts */
            snprintf(iso_out, iso_size, "%s", uri);
            return 1;
        }
        fprintf(stderr, "android: can't be read in place (%s); trying a copy\n", why);
        if (copy_disc(uri, copy) && !(why = host_disc_check(copy))) {
            snprintf(iso_out, iso_size, "%s", copy);
            return 1;
        }
        snprintf(message, sizeof message, "%s%s\n\nThe file couldn't be read or copied (is there "
                 "room on the phone?). Choose your Downhill Domination (USA) disc image "
                 "(SCUS-97177) again.", why ? why : "", why ? "\n\n" : "");
    }
}

/* dhd.ini names the disc (the content URI picked in place, or the copy; host_disc_ready checks it
   in the core; DHD_ISO would skip that check). A changed choice overwrites the old one — a new
   pick, or a copy that replaced a URI that couldn't be read. The player defaults apply as soon as
   dhd.ini exists (host_settings_load). */
static void settings_start(const char *disc)
{
    char path[1200], cur[4096];
    snprintf(path, sizeof path, "%sdhd.ini", files);
    if (!dhd_settings_read(path, "iso", cur, sizeof cur) || strcmp(cur, disc))
        dhd_settings_save(path, "iso", disc);
#if defined(__arm__)
    /* Default to the lightest resolution only when the player has not chosen one.
       Existing preferences and explicit DHD_SCALE overrides remain untouched. */
    if (!getenv("DHD_SCALE") && !dhd_settings_read(path, "scale", cur, sizeof cur)) {
        if (dhd_settings_save(path, "scale", "1"))
            fprintf(stderr, "android: no chosen resolution; using 1x for ARMv7 (change in touch settings)\n");
    }
#endif
}

__attribute__((visibility("default"))) int SDL_main(int argc, char **argv)
{
    boot_start_ms = SDL_GetTicks();
    /* the guest span before anything grows into the address space (32-bit: the reservation
       that decides whether the game fits at all) */
    {
        extern void guest_memory_reserve_early(void);
        guest_memory_reserve_early();
    }
    const char *storage = SDL_GetAndroidInternalStoragePath();
    if (!storage) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "no internal storage (%s)", SDL_GetError());
        return 1;
    }
    snprintf(files, sizeof files, "%s/", storage);
    snprintf(exe, sizeof exe, "%sdhd64", files);
    /* the log's copy in the external files folder, where a file manager can reach it
       (Android/data/app.downhillrevived/files/dhd.log): set before the log opens */
    {
        const char *ext = SDL_GetAndroidExternalStoragePath();
        static char mirror[1200];
        if (ext) {
            snprintf(mirror, sizeof mirror, "%s/dhd.log", ext);
            setenv("DHD_LOG_MIRROR", mirror, 1);
        }
    }
    host_console_init();
    fprintf(stderr, "[BUILD] Android commit=%s; split-memory=%s\n", DHD_BUILD_ID,
#ifdef DHD_EXPERIMENTAL_SPLIT_MEMORY
            "experimental"
#else
            "off"
#endif
    );
    android_boot_note("App started; log opened");
#ifdef DHD_EXPERIMENTAL_SPLIT_MEMORY
    {
        extern uintptr_t guest_base;
        fprintf(stderr,"android: experimental split guest reservation %s (base=%p)\n",
                guest_base ? "ready" : "not ready; will retry",(void *)guest_base);
    }
#endif
    fprintf(stderr, "android: Downhill Revived in %s\n", files);
    install_crash_report();  /* the log is open now; the game's fault guard chains to it */
    {   /* a profile-collecting build (build_host64.py --target android-arm64 --pgo-generate): its counters go to the
           files folder (host_exit writes them; the process ends with _exit, so no atexit runs) */
        extern void __llvm_profile_set_filename(const char *) __attribute__((weak));
        static char profile[1100];
        snprintf(profile, sizeof profile, "%sdhd-%%p.profraw", files);
        if (__llvm_profile_set_filename) __llvm_profile_set_filename(profile);
    }
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    char disc[4096];
    android_boot_note("Checking game disc; a file picker may appear");
    if (!disc_ready(disc, sizeof disc)) {
        fprintf(stderr, "[BOOT] No usable disc selected; exiting.\n");
        exit(0);
    }
    android_boot_note("Disc readable; starting game core");
    settings_start(disc);
    atomic_store_explicit(&settings_ready, 1, memory_order_release);
    char *args[] = {exe, NULL};
    (void)argc; (void)argv;
    int code = dhd_launcher_main(1, args);
    fprintf(stderr, "android: the game ended (%d)\n", code);
    exit(code);
}
