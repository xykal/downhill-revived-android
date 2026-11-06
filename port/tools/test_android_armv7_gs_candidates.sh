#!/usr/bin/env bash
# Synthetic ARM Linux/QEMU regressions for the 32-bit fork. Not an Android GPU,
# actual game, performance benchmark, or proof of gameplay compatibility.
set -euo pipefail
cd "$(dirname "$0")/../.."
sdl=${SDL3_SOURCE:-/tmp/SDL3-3.4.18}
[ -f "$sdl/CMakeLists.txt" ] || { echo "SDL source missing: $sdl" >&2; exit 1; }
cc=arm-linux-gnueabihf-gcc
inc=(-I"$sdl/include" -Iport/render/include -Iport/runtime -Iport/include -Iport/host -Iport/hle)
"$cc" -std=gnu11 -O2 -ffp-contract=off -fno-strict-aliasing -ffunction-sections -fdata-sections -w "${inc[@]}" \
  -c port/gs/test_native_strip_cpu.c -o /tmp/dhd-strip.o
"$cc" -static -no-pie -Wl,--gc-sections /tmp/dhd-strip.o port/gs/memory.c port/gs/gs.c \
  port/gs/test_native_strip_stubs.c -lm -lpthread -ldl -o /tmp/dhd-strip
qemu-arm -cpu cortex-a15 /tmp/dhd-strip 2000
if qemu-arm -cpu cortex-a15 /tmp/dhd-strip 10 negative; then
  echo 'FAIL: strip negative control was not detected' >&2; exit 1
fi
echo 'strip negative control: detected'
cmake -S "$sdl" -B /tmp/dhd-sdl-arm -G Ninja \
  -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=arm -DCMAKE_C_COMPILER="$cc" \
  -DSDL_UNIX_CONSOLE_BUILD=ON -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF \
  -DSDL_TESTS=OFF -DSDL_VIDEO=OFF -DSDL_AUDIO=OFF -DSDL_JOYSTICK=OFF -DSDL_HAPTIC=OFF \
  -DSDL_HIDAPI=OFF -DSDL_SENSOR=OFF -DSDL_CAMERA=OFF -DSDL_RENDER=OFF >/tmp/dhd-sdl-config.log
cmake --build /tmp/dhd-sdl-arm -j4 >/tmp/dhd-sdl-build.log
"$cc" -std=gnu11 -O2 -ffp-contract=off -static -no-pie -DGRAPHICS_QUEUE_TEST -DGS_QUEUE_TEST "${inc[@]}" \
  port/hle/test_queued_store_arm.c port/hle/graphics.c port/hle/graphics_queue.c port/gs/memory.c port/gs/gs.c \
  /tmp/dhd-sdl-arm/libSDL3.a -lm -lpthread -ldl -o /tmp/dhd-store
for gs in 0 1; do
  for page in 4096 16384; do
    DHD_MTVU=1 DHD_GS_WORKER=$gs DHD_SPIN_US=0 qemu-arm -cpu cortex-a15 /tmp/dhd-store "$page"
    DHD_MTVU=1 DHD_GS_WORKER=$gs DHD_QUEUE_READS=0 DHD_SPIN_US=0 qemu-arm -cpu cortex-a15 /tmp/dhd-store "$page"
  done
done
DHD_MTVU=0 DHD_GS_WORKER=0 DHD_SPIN_US=0 qemu-arm -cpu cortex-a15 /tmp/dhd-store 16384
DHD_MTVU=1 DHD_GS_WORKER=1 DHD_SYNC_STRICT=1 DHD_SPIN_US=0 qemu-arm -cpu cortex-a15 /tmp/dhd-store 16384
