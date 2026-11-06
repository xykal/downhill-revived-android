# 129 — ARMv7 selective upstream graphics candidate

This is an experimental 32-bit fork, not a measurement of a Redmi A2 or a guarantee of faster gameplay. It preserves the native renderer, visual settings, URI/save behavior and experimental split guest mapping.

Selective upstream changes: `4fb0c33` (PACKED decode, after this fork's earlier `d477022`), `d48789b`, `606be3a`, `1415e21`, `09291c7`, `3ae5282`, `b9767d9`, `4435f9a`, `f7f7307` (strip recording and decoding), and `dcf09c9` (Z readback copy). Strip decoder differential comes from `4f73e6b`. `00d5fe6` changes the repeated-wake spin budget. `2e8185a` queues store-image reads in drawing order and `d5073e5` copies the source before guarding the destination, including 16K page overlap. Upstream reports its own Windows race matching and performance tests on a throttled Mi 11i; those are not results on ARMv7 or this Redmi.

The fork's graphics timeline merge retains its `tl_site` call and the new `csr_mark`. The browser keeps its existing imports plus the queued-read fallback stubs. The wasm2c bridge checks the source span and destination span for the new service.

## Synthetic tests

`test_android_armv7_gs_candidates.sh` builds tests for ARM Linux and runs them under QEMU, **not** on Android. It tests the actual `native_draw_strip` recorder against `native_draw` on pre-initialized state, followed by another draw. It compares vertices, indices, bounding box, counters and the surviving conversion cache for randomized flat/smooth, indexed/unindexed, FST/STQ and skipped triangles. A corruption control must fail. It deliberately traps if the test touches GPU, texture or other unexercised paths; it does not validate rendering, commands or state changes requiring the backend. Ten thousand batches plus following draws matched locally; CI runs 2,000.

The second test uses actual `graphics.c`, both FIFO workers, real GS and real Linux page protections, but **models** VIF DIRECT and calls the guest code as native ARM C (not wasm2c). Each mode reads 100 packets with packet and destination sharing a page, then 17 queued packets to exercise the 16-slot limit, including reading via RAM aliases. Runs cover one or two workers, disabled queued reads, no workers, strict sync, and 4K/16K simulated protection. All outputs matched. A separate negative test removing `d5073e5` failed on the first overlapping read as expected. This is not a full 16K Android page, signal/wasm bridge or PowerVR test.

Existing GS upload fuzz and the upstream 64 randomized strip decoder tests passed on host and QEMU, as did the fork's native frontend metadata test. These do not establish phone FPS, scene quality, audio improvement or general correctness. The Android ARMv7 CI must still compile and validate the release-signed APK. Test by installing over the existing same-signed app, never uninstall or clear data. If performance is unchanged or the game fails on the device, do not infer a successful optimization from these tests.
