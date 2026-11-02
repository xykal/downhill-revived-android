"""Compile and execute a tiny generated wasm2c module on real split mappings.

No game data. --arm runs a statically linked 32-bit ARM Linux binary via
qemu-arm; this is NOT Android or the production game's allocator.
"""
import argparse
import resource
import subprocess
import sys
import tempfile
from pathlib import Path
from prepare_wasm2c import prepare

ROOT = Path(__file__).resolve().parents[2]
HOST = ROOT / 'port/host64'


def run(args, **kwargs):
    print('+', ' '.join(map(str, args)), flush=True)
    return subprocess.run(list(map(str, args)), check=True, **kwargs)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--wabt', type=Path, required=True)
    p.add_argument('--simde', type=Path, required=True)
    p.add_argument('--arm', action='store_true')
    p.add_argument('--clang', action='store_true', help='native Clang, with real __builtin_assume')
    p.add_argument('--negative-control', action='store_true')
    a = p.parse_args()
    rt = a.wabt / 'share/wabt/wasm2c'
    with tempfile.TemporaryDirectory(prefix='dhd-split-test-') as tmp:
        out = Path(tmp)
        run([a.wabt/'bin/wat2wasm', HOST/'test_split_module.wat', '-o', out/'core.wasm'])
        run([a.wabt/'bin/wasm2c', out/'core.wasm', '-n', 'core', '-o', out/'core.c'])
        prepare(out/'core.c')
        prepared = (out/'core.c').read_text()
        assert 'host64_base_ok(' in prepared and 'host64_memory_copy(' in prepared
        cc = 'clang' if a.clang else ('arm-linux-gnueabihf-gcc' if a.arm else 'cc')
        flags = ['-std=gnu11', '-O3', '-fwrapv', '-fno-strict-aliasing', '-ffunction-sections',
                 '-fdata-sections', '-Wno-unused-function', '-DWASM_RT_USE_SEGUE=0',
                 '-DWASM_RT_MEMCHECK_GUARD_PAGES=0', '-DWASM_RT_MEMCHECK_BOUNDS_CHECK=1',
                 '-I'+str(a.wabt/'include'), '-I'+str(rt), '-I'+str(HOST), '-I'+str(out), '-I'+str(a.simde)]
        if not a.clang:
            flags += ['-D__builtin_assume(x)=((void)0)']
        if a.arm:
            flags += ['-static', '-march=armv7-a', '-mfpu=vfpv3-d16']
            if a.clang:
                flags += ['--target=arm-linux-gnueabihf', '--gcc-toolchain=/usr']
        sources = [out/'core.c', rt/'wasm-rt-impl.c', HOST/'split_reservation.c',
                   HOST/'guest_memory.c', HOST/'guest_memory_posix.c', HOST/'bridge.c',
                   ROOT/'port/host/host_posix.c',
                   HOST/'test_split_module.c']
        exe = out/'split-test'
        run([cc, *flags, '-DDHD_EXPERIMENTAL_SPLIT_MEMORY', *sources, '-Wl,--gc-sections', '-pthread', '-ldl', '-lm', '-o', exe])
        prefix = ['qemu-arm'] if a.arm else []
        run([*prefix, exe], timeout=30)
        if a.negative_control:
            unsafe = out/'split-test-unsafe'
            run([cc, *flags, '-DDHD_EXPERIMENTAL_SPLIT_MEMORY',
                 '-DDHD_SPLIT_TEST_DISABLE_ACCESS_GUARD', *sources, '-Wl,--gc-sections', '-pthread', '-ldl', '-lm', '-o', unsafe])
            def no_core():
                resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
            result = subprocess.run([*prefix, str(unsafe)], capture_output=True, text=True,
                                    timeout=30, preexec_fn=no_core)
            if result.returncode == 0 or 'expected trap' not in result.stderr:
                raise RuntimeError('unsafe negative control did not expose the missing hole trap: '
                                   + result.stderr[-1000:])
            print('PASS: unsafe build fails the expected-hole-trap negative control', flush=True)


if __name__ == '__main__':
    main()
