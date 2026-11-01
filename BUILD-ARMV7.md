# Building for Android armeabi-v7a (32-bit)

This tree adds a 32-bit Android target to the port: `android-armv7` (armeabi-v7a, Android 8.0+ /
API 26) alongside upstream's `android-arm64`. It exists for budget MediaTek and other 32-bit
SoCs (finding 126). Both ABIs build; the APKs are separate (the game is BYOD either way).

What changed, by file:

- `port/tools/build_host64.py` — the `android-armv7` target (armv7-a + NEON, softfp, tuned for
  Cortex-A53-class cores), 32-bit `guest_base` semantics: the guest span ends at `0x80000000`
  (every region the layout reaches under 2 GiB) with **WABT software bounds checks** instead of
  guard pages (a 32-bit address space can neither reserve the span nor fault a wrapped
  pointer), per-ABI `lib/<abi>/`, `-z max-page-size=4096` (16 KiB pages are arm64-only), the
  NDK tool directory picked per host OS, `llvm-strip` without `.exe` on POSIX, `--fake` allowed
  for compile checks on either ABI, and the VU1 AOT (`--fake`-only fixture) guarded so a real
  build without the dump fails loudly.
- `port/tools/sdl3.py` — `android_build(abi, api)` (armeabi-v7a at API 26).
- `port/tools/rustgs.py` — the `armv7-linux-androideabi` target: per-host NDK tools, and
  `CFLAGS --target=armv7a-linux-androideabi26 -marm` (ring's `.S` files assemble as ARM, not
  Thumb; the NDK's cc-derived target has no API level).
- `port/tools/check_rust_lib.py` + `port/render/native-static-libs-armv7-linux-androideabi.txt`
  — the armv7 allow-list.
- `port/render/src/guard.rs` — FPSCR (RMode/FZ sit where FPCR's do; no AArch32 FPSR).
- `port/render/src/abi.rs`, `src/device.rs`, `include/dhd_render.h`, `port/gs/gpu_rust.c` —
  `DHDR_DEV_GL` / `DHD_WGPU_BACKEND=gl`: OpenGL ES 3.0 for phones whose Vulkan is missing or
  broken (budget MediaTek/PowerVR).
- `port/host64/fiber_arm32.c` (new) — the fiber switch for AAPCS32 (r4-r11 + lr, d8-d15,
  FPSCR), 112-byte frame, 8-aligned.
- `port/host64/{fp_mode,selftest_fiber,guest_memory,guest_memory_posix}.c`,
  `port/host/host_posix.c`, `port/hle/{host,graphics_queue,vif}.c`, `port/runtime/{portable.h,vu0_interp.c}`,
  `port/include/guest.h`, `port/gs/native_core.inc` — the `__arm__` paths: FPSCR/IEEE modes,
  ucontext arm32 registers, SIMDe SSE2→NEON, `memfd_create` via syscall 385 (bionic exports it
  from API 30 only), `DHD_SCALE` default 1 on 32-bit Android (Mali-G5x loses ~2/3 of frame rate
  at 2x, finding 126), and `vu0_interp.c`'s VU1 AOT hook for `__arm__`.
- `port/android/` — `--abi` on `build_apk.py` (per-ABI `minSdk` 26/30, `jniLibs/<abi>`,
  manifest placeholders: Vulkan required on arm64, optional on armv7 with GLES3 required).

## Local build (what CI runs)

```sh
# tools: JDK 17, Android SDK (platform 35, build-tools 35), NDK r27c, Rust with
#   armv7-linux-androideabi, wasi-sdk 34, wabt 1.0.42, simde @65c15e6, cmake+ninja (pip).
#   The port's own tooling fetches SDL3 and the pinned registry into build/ and ~/.cache.
export ANDROID_NDK_HOME=…/android-ndk-r27c
export JAVA_HOME=…/jdk-17 … ANDROID_HOME=…/android-sdk
export GRADLE_USER_HOME=… ANDROID_USER_HOME=$GRADLE_USER_HOME/au
export DHD_DEVCACHES=…   # symlinks: wabt/wabt-1.0.42, simde-src (and the SDL/rustgs caches)

# Your own disc dump: only SCUS_971.77 (the ELF) is needed — no ISO.
export DHD_VUTEXT=/path/to/SCUS_971.77      # enables VU1 AOT; without it only --fake links

python3 port/tools/build_host64.py --target android-armv7 \
    --wasi …/wasi-sdk-34.0-x86_64-linux --jobs 4      # build/port/dhd_core.wasm + libmain.so
python3 port/android/build_apk.py --abi armeabi-v7a   # → build/android/*-armeabi-v7a-debug.apk
```

`--fiber-test` builds `selftest_fiber` for the target (run it on a device: `adb push` +
`--guest-guard --native-guard`). `--fake` links the fixture without the dump (compile check).

## CI

`.github/workflows/build-android.yml` builds the armeabi-v7a APK on every push (and
optionally arm64). Set the repository secret `DHD_ELF_URL` to an HTTPS URL of your own
`SCUS_971.77` dump (it never touches the repo); without it the workflow runs the fixture
build as a smoke test. Debug APKs come from the Actions artifacts.

## MediaTek notes

- Prefer `DHD_WGPU_BACKEND=gl` if the device's Vulkan driver misbehaves (manifest allows
  GLES3 on armv7).
- The default native scale is 1x on 32-bit Android; set `DHD_SCALE=2` only if the device
  holds 30 fps (finding 126: Mali-G5x ~10 fps at 2x).
