#!/usr/bin/env python3
"""Ensure runtime-selected GLES is actually enabled in the Android ARMv7 build.

Inspects Cargo's resolved target-specific feature graph rather than merely
checking that the manifest contains a string. No GPU or game files required.
"""
from pathlib import Path
import subprocess

RENDER = Path(__file__).resolve().parents[1] / 'render'


def tree(target):
    return subprocess.check_output([
        'cargo', 'tree', '--locked', '--offline', '-p', 'dhd_render', '--target', target,
        '--features', 'native', '-e', 'features', '-i', 'wgpu-hal',
    ], cwd=RENDER, text=True)


def main():
    arm = tree('armv7-linux-androideabi')
    for feature in ('wgpu feature "gles"', 'wgpu-core feature "gles"', 'wgpu-hal feature "gles"'):
        if feature not in arm:
            raise SystemExit('FAIL: ARMv7 renderer lacks '+feature)
    arm64 = tree('aarch64-linux-android')
    if 'wgpu-hal feature "gles"' in arm64:
        raise SystemExit('FAIL: GLES unintentionally enabled in Android arm64 build')
    print('PASS: ARMv7 resolves wgpu/core/HAL GLES; arm64 remains unchanged')


if __name__ == '__main__':
    main()
