#!/usr/bin/env python3
"""Run the real arm32 fiber adapter on 32-bit ARM Linux/QEMU, not Android.

Uses both Clang and GCC and both Thumb and ARM code. The optional negative
controls reintroduce the overlapping FP save and the broken IEEE-mode reset;
the same test must reject both. No game files are used.
"""
import argparse
import resource
import signal
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HOST = ROOT / 'port/host64'


def run(cmd):
    print('+', ' '.join(map(str, cmd)), flush=True)
    return subprocess.run(list(map(str, cmd)), capture_output=True, text=True, timeout=90)


def compile_test(out, wabt, cc, mode, fiber, fp):
    compiler = ['clang', '--target=arm-linux-gnueabihf', '--gcc-toolchain=/usr'] if cc == 'clang' else ['arm-linux-gnueabihf-gcc']
    cmd = [*compiler, '-std=gnu11', '-O3', '-fwrapv', '-fno-strict-aliasing', '-static', '-pthread',
           '-march=armv7-a', '-mfpu=vfpv3-d16', '-m'+mode, '-fomit-frame-pointer', '-funwind-tables',
           '-DWASM_RT_STACK_DEPTH_COUNT=1', '-DDHD_EXPERIMENTAL_SPLIT_MEMORY',
           '-I'+str(wabt/'include'), '-I'+str(HOST), HOST/'selftest_fiber.c', HOST/'guest_memory.c',
           HOST/'guest_memory_posix.c', HOST/'split_reservation.c', HOST/'fiber.c', fiber, fp,
           '-ldl', '-lm', '-o', out]
    p = run(cmd)
    if p.returncode:
        raise RuntimeError('compile failed:\n'+p.stdout+p.stderr)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--wabt', type=Path, required=True)
    p.add_argument('--negative-control', action='store_true')
    args = p.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    with tempfile.TemporaryDirectory(prefix='dhd-fiber-arm-') as td:
        td = Path(td)
        for cc, mode in [('clang', 'thumb'), ('clang', 'arm'), ('gcc', 'thumb'), ('gcc', 'arm')]:
            exe = td/(cc+'-'+mode)
            compile_test(exe, args.wabt, cc, mode, HOST/'fiber_arm32.c', HOST/'fp_mode.c')
            r = run(['qemu-arm', exe])
            if r.returncode or 'PASS: fibers:' not in r.stdout:
                raise RuntimeError('fiber test failed:\n'+r.stdout+r.stderr)
            print(r.stdout.strip(), '('+cc+', '+mode+')', flush=True)
            for flag, expected in [('--guest-guard', 'uncommitted guest address 0x006000000'),
                                   ('--native-guard', 'native stack: 8 MiB less 64 KiB written')]:
                r = run(['qemu-arm', exe, flag])
                if r.returncode not in (-signal.SIGSEGV, -signal.SIGBUS, 128+signal.SIGSEGV, 128+signal.SIGBUS) or expected not in r.stderr:
                    raise RuntimeError(flag+' did not fault as expected:\n'+r.stdout+r.stderr)
                print('PASS:', cc, mode, flag, 'faults after the expected diagnostic', flush=True)
        if args.negative_control:
            good = (HOST/'fiber_arm32.c').read_text()
            broken = good.replace('"  add r3, sp, #40\\n"\n"  vstmia r3, {d8-d15}\\n"', '"  vstmia sp, {d8-d15}\\n"')
            broken = broken.replace('"  add r3, sp, #40\\n"\n"  vldmia r3, {d8-d15}\\n"', '"  vldmia sp, {d8-d15}\\n"')
            if broken == good or 'vstmia sp, {d8-d15}' not in broken or 'vldmia sp, {d8-d15}' not in broken:
                raise RuntimeError('could not construct prior broken fiber adapter')
            fiber = td/'fiber_broken.c'
            fiber.write_text(broken)
            exe = td/'fiber-negative'
            compile_test(exe, args.wabt, 'clang', 'thumb', fiber, HOST/'fp_mode.c')
            r = run(['qemu-arm', exe])
            if r.returncode == 0:
                raise RuntimeError('overlapping-register negative control unexpectedly passed')
            print('PASS: old overlapping fiber frame is rejected', flush=True)
            fp_source = (HOST/'fp_mode.c').read_text()
            old = 'uint32_t fpscr=0;  /* IEEE: round to nearest, no FZ, status flags clear. */'
            bad = 'uint32_t fpscr; __asm__ volatile("vmrs %0, fpscr" : "=r"(fpscr)); fpscr &= 0x0FFFFFFFu;'
            if old not in fp_source:
                raise RuntimeError('could not construct prior IEEE reset')
            fp = td/'fp_broken.c'
            fp.write_text(fp_source.replace(old, bad))
            exe = td/'fp-negative'
            compile_test(exe, args.wabt, 'clang', 'thumb', HOST/'fiber_arm32.c', fp)
            r = run(['qemu-arm', exe])
            if r.returncode == 0 or 'fp_control()' not in r.stderr:
                raise RuntimeError('IEEE reset negative control was not rejected:\n'+r.stdout+r.stderr)
            print('PASS: old IEEE-mode reset is rejected', flush=True)
    print('PASS: ARM Linux/QEMU fiber regressions (not Android)', flush=True)


if __name__ == '__main__':
    main()
