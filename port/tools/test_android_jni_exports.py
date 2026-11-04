#!/usr/bin/env python3
"""Link tiny Android JNI fixture through the actual production export map.
Run this before the expensive full Android build, not just nm on a relocatable object.
"""
import ast
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = ast.parse((root / 'port/tools/build_host64.py').read_text())
exports = next(ast.literal_eval(node.value) for node in source.body
               if isinstance(node, ast.Assign) and any(isinstance(x, ast.Name) and x.id == 'ANDROID_EXPORTS' for x in node.targets))
bin_dir = Path(os.environ['ANDROID_NDK_HOME']) / 'toolchains/llvm/prebuilt/linux-x86_64/bin'
with tempfile.TemporaryDirectory() as tmp:
    p = Path(tmp)
    (p / 'exports.map').write_text(exports)
    subprocess.run([str(bin_dir / 'armv7a-linux-androideabi26-clang'), '-std=gnu11', '-fPIC', '-shared',
                    '-Wl,--no-undefined', '-Wl,--version-script=' + str(p/'exports.map'),
                    '-I' + str(root/'port/include'), '-I' + str(root/'port/runtime'),
                    str(root/'port/host/android_touch.c'), str(root/'port/host/test_android_jni_link_stubs.c'),
                    '-o', str(p/'libfixture.so')], check=True)
    output = subprocess.check_output([str(bin_dir/'llvm-nm'), '-D', '--defined-only', str(p/'libfixture.so')], text=True)
    for name in ('nativeTouchState', 'nativeSaveOption', 'nativeSettingsLoaded', 'nativeReadOption'):
        symbol = 'Java_app_downhillrevived_DhdActivity_' + name
        if symbol not in output:
            raise SystemExit('Missing JNI export: ' + symbol)
    print('PASS: production ARMv7 link export map exposes all settings JNI symbols')
