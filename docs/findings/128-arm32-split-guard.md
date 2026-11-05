# ARM32 experimental split-memory hot guard

The split-memory wasm2c fast path checks a 16-byte span and guest RAM alias
windows on each generated scalar/SIMD access. On ARM32 Clang 19 (-O3,
Cortex-A53 tuning), the old generic 64-bit span arithmetic generated long
carry chains in a standalone `host64_base_ok(base,0/4)` test. The replacement
uses subtraction-before-addition to prove the effective address is inside a
mapped segment and below 2 GiB, then tests the same alias mask with 32-bit
operations. It retains the exact conservative 16-byte span and existing slow
path. Other, non-split builds retain the previous code.

Correctness: `test_split_access.c` compares the complete new decision with a
reference of the former code across segment and alias boundaries, overflow and
5 million deterministic randomized cases; run on x86_64, ARM QEMU, and under
ASan/UBSan. The generated wasm2c fixture exercises actual split mappings,
hole sentinels, slow access and a deliberately unsafe negative control; it
passed on Linux Clang and ARM Clang/QEMU. The fixture is not production Android.

A *synthetic* ARM Clang/QEMU microbenchmark (`bench_split_access.c`) used
constant offsets 0, 4 and 0x70000000 with simulated reads and a small slow-path
function. Three interleaved runs (seconds, medians):

| synthetic address mix | old | new |
|---|---:|---:|
| low RAM | 0.721 | 0.441 |
| low RAM with 10% alias window | 0.748 | 0.527 |
| low/alias/high mix | 1.013 | 0.648 |
| uniformly random 32-bit bases | 1.248 | 1.067 |

Checksums match. The benchmark is not actual wasm2c game code, QEMU timing is
not Cortex-A53 hardware timing, address distributions are invented, and no
renderer, audio or game workload is included. Consequently these results do
not predict FPS or prove any gain on the Redmi A2. Measure actual hardware;
keep the previous APK available until build and signing checks succeed.
