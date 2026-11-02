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
    ruff = shutil.which('ruff')
    tracked = subprocess.run(['git', 'ls-files', '--cached', '--others', '--exclude-standard', '*.py'], cwd=ROOT,
                             capture_output=True, text=True)
    ruff_paths = [p for p in tracked.stdout.split() if (ROOT/p).is_file()]
    if ruff:
        run('python lint (F821,E9)',
            [ruff, 'check', '--select', 'F821,E9', *ruff_paths], env=env)
    else:
        print('SKIP: python lint (ruff not installed; pip install -r '
              'tools/requirements-dev.txt)', file=sys.stderr)
    for entry in ('tools/build/pw3-os-trial.py',
                  'tools/maintenance/kindle-linux.py',
                  'tools/maintenance/stock-capture.py'):
        run('entry point --help ('+entry+')',
            [sys.executable, str(ROOT/entry), '--help'], env=env)
    shell_tracked = subprocess.run(
        ['git', 'ls-files', '--cached', '--others', '--exclude-standard', '*.sh', '*.in.sh', '*.in.conf'],
        cwd=ROOT, capture_output=True, text=True)
    shell_files = sorted(str(ROOT/p) for p in shell_tracked.stdout.split()
                         if (ROOT/p).is_file())
    run('shell syntax', ['sh', '-n', *shell_files])
    run('build flag policy',
        [sys.executable, str(ROOT/'tools/check_paths.py'), 'flags'], env=env)
    run('hardcoded path policy',
        [sys.executable, str(ROOT/'tools/check_paths.py'), 'paths'], env=env)
    run('documentation contract',
        [sys.executable, str(ROOT/'tools/docs/check.py')], env=env)
    run('theme check', [sys.executable, str(ROOT/'theme/generate.py'), '--check'])
    run('public-source roundtrip',
        [sys.executable, str(ROOT/'tools/package/public-source.py'), '--check'],
        env=env)
    run('system syntax (lifecycle 0 and 1)',
        ['make', '-C', str(ROOT/'os/system'), 'check-both', f'CC={cc}'], env=env)
    run('system host tests',
        ['make', '-C', str(ROOT/'os/system'), 'test', f'CC={cc}',
         f'HOSTCC={cc}'], env=env)
    strace = shutil.which('strace')
    if strace:
        with tempfile.NamedTemporaryFile() as trace:
            result = subprocess.run(
                [strace, '-f', '-e', 'trace=fsync,rename,renameat,renameat2',
                 '-o', trace.name, str(ROOT/'os/system/test-stage3')],
                cwd=ROOT/'os/system', capture_output=True, text=True)
            text = Path(trace.name).read_text(errors='replace')
        lines = [line for line in text.splitlines()
                 if 'fsync(' in line or 'rename(' in line]
        rename_at = max((i for i, line in enumerate(lines)
                         if 'rename(' in line), default=-1)
        fsync_before = any('fsync(' in line for line in lines[:rename_at])
        if result.returncode != 0 or rename_at < 0 or not fsync_before:
            FAILURES.append('atomic write fsync order')
            print('FAIL: atomic write did not fsync before rename',
                  file=sys.stderr)
        else:
            print('==> atomic write: fsync precedes rename (strace)')
    else:
        print('SKIP: atomic write strace proof (strace not installed)',
              file=sys.stderr)
    run('tooling safety tests',
        [sys.executable, '-O', '-m', 'unittest', 'discover', '-s',
         str(ROOT/'tools/tests')], env=env)
    run('EPDC policy tests',
        ['make', '-C', str(ROOT/'os/ui'), 'test-epdc-policy', f'HOSTCC={cc}'], env=env)
    run('EPDC batch tests',
        ['make', '-C', str(ROOT/'os/ui'), 'test-epdc-batch', f'HOSTCC={cc}'], env=env)
    run('paint profile tests',
        ['make', '-C', str(ROOT/'os/ui'), 'test-paint-profile', f'HOSTCC={cc}'], env=env)
    run('paint rule tests',
        ['make', '-C', str(ROOT/'os/ui'), 'test-paint-rules', f'HOSTCC={cc}'], env=env)
    if (ROOT/'work/sources/lvgl').is_dir():
        run('UI production syntax',
            ['make', '-C', str(ROOT/'os/ui'), 'check', f'CC={cc}'], env=env)
        run('UI host simulator build',
            ['make', '-C', str(ROOT/'os/ui'), 'host-sim', f'HOSTCC={cc}'], env=env)
        with tempfile.TemporaryDirectory() as tmp:
            run('UI geometry suite',
                [str(ROOT/'os/ui/host-sim'), tmp], env=env)
        with tempfile.TemporaryDirectory() as tmp:
            crash = subprocess.run(
                [str(ROOT/'os/ui/host-sim'), tmp], env=dict(env,
                                                             GW_UI_TEST_CRASH='1'),
                capture_output=True, text=True, timeout=10)
            if crash.returncode not in (-11, 139):
                FAILURES.append('UI crash handler')
                print('FAIL: UI crash handler did not exit on SIGSEGV: %r'
                      % crash.returncode, file=sys.stderr)
            else:
                print('==> UI crash handler: prompt exit %d' % crash.returncode)
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
