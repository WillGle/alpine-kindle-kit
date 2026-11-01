#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Root check runner: every regression gate that does not need a device."""
from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FAILURES: list[str] = []


def run(name, argv, cwd=ROOT, env=None, allow_missing=False):
    print(f'==> {name}: {" ".join(str(x) for x in argv)}')
    result = subprocess.run([str(x) for x in argv], cwd=cwd, env=env,
                            capture_output=True, text=True)
    if result.stdout.strip():
        print(result.stdout.rstrip())
    if result.returncode != 0:
        if result.stderr.strip():
            print(result.stderr.rstrip(), file=sys.stderr)
        if allow_missing:
            print(f'SKIP: {name} (toolchain unavailable)', file=sys.stderr)
            return
        FAILURES.append(name)
        print(f'FAIL: {name}', file=sys.stderr)


def main():
    cc = os.environ.get('CC') or shutil.which('cc') or shutil.which('gcc')
    if not cc:
        raise SystemExit('No host C compiler found (set CC=)')
    env = dict(os.environ, CC=cc, HOSTCC=cc, SOURCE_DATE_EPOCH='0')

    py_files = sorted(p for p in (ROOT/'tools').rglob('*.py'))
    py_files += sorted((ROOT/'theme').glob('*.py'))
    py_files += sorted((ROOT/'os/system/tests').glob('*.py'))
    run('python compileall', [sys.executable, '-m', 'compileall', '-q',
                              *[str(p) for p in py_files]])
    shell_files = sorted(p for p in (ROOT/'os').rglob('*.sh'))
    shell_files += sorted(p for p in (ROOT/'tools').rglob('*.sh'))
    shell_files += sorted(p for p in (ROOT/'vm').rglob('*.sh'))
    shell_files += sorted((ROOT/'os/devices/pw3/bootstrap').glob('*.in.sh'))
    shell_files += [ROOT/'os/devices/pw3/bootstrap/return-control.in.conf']
    run('shell syntax', ['sh', '-n', *[str(p) for p in shell_files]])
    run('theme check', [sys.executable, str(ROOT/'theme/generate.py'), '--check'])
    run('public-source roundtrip',
        [sys.executable, str(ROOT/'tools/package/public-source.py'), '--check'],
        env=env)
    run('system syntax (lifecycle 0 and 1)',
        ['make', '-C', str(ROOT/'os/system'), 'check-both', f'CC={cc}'], env=env)
    run('system host tests',
        ['make', '-C', str(ROOT/'os/system'), 'test', f'CC={cc}',
         f'HOSTCC={cc}'], env=env)
    run('tooling safety tests',
        [sys.executable, '-O', '-m', 'unittest', 'discover', '-s',
         str(ROOT/'tools/tests')], env=env)
    run('EPDC policy tests',
        ['make', '-C', str(ROOT/'os/ui'), 'test-epdc-policy', f'HOSTCC={cc}'], env=env)
    run('paint profile tests',
        ['make', '-C', str(ROOT/'os/ui'), 'test-paint-profile', f'HOSTCC={cc}'], env=env)
    if (ROOT/'work/sources/lvgl').is_dir():
        run('UI production syntax',
            ['make', '-C', str(ROOT/'os/ui'), 'check', f'CC={cc}'], env=env)
        run('UI host simulator build',
            ['make', '-C', str(ROOT/'os/ui'), 'host-sim', f'HOSTCC={cc}'], env=env)
        with tempfile.TemporaryDirectory() as tmp:
            run('UI geometry suite',
                [str(ROOT/'os/ui/host-sim'), tmp], env=env)
        run('UI pre-fix regressions',
            [sys.executable, str(ROOT/'os/ui/tests/prefix_regression.py')], env=env)
        arm = shutil.which('arm-linux-musleabihf-gcc')
        if arm:
            run('UI ARM build',
                ['make', '-C', str(ROOT/'os/ui'), 'gw-shell',
                 f'CC={arm}', f'STRIP=arm-linux-musleabihf-strip'], env=env)
    else:
        print('SKIP: UI suite (work/sources/lvgl not present)')

    if FAILURES:
        print('Check failures: ' + ', '.join(FAILURES), file=sys.stderr)
        return 1
    print('CHECK PASSED')
    return 0


if __name__ == '__main__':
    sys.exit(main())
