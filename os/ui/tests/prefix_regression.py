#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prove the Phase 3 regressions fail against the old behaviour.

Each case copies the current UI tree, reintroduces one pre-fix behaviour by a
small source mutation, builds the host suite and requires the corresponding
check to fail.  The unmodified tree must pass.  Run inside the host toolchain
environment (gcc/make available); LVGL is taken from the repository work tree.
"""
from __future__ import annotations

import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
UI = ROOT / 'os/ui'
LVGL = ROOT / 'work/sources/lvgl'

MUTATIONS = [
    ('ipc failure response is not cleared',
     'gw_ipc_client.inc',
     "    resp[0] = '\\0';\n    if (g_host_ipc_fail_prefix",
     "    if (g_host_ipc_fail_prefix",
     'failure_clears_response'),
    ('todo commit does not refresh Home',
     'gw_todo_store.inc',
     '    todo_rebuild_list();\n    apply_model();',
     '    todo_rebuild_list();',
     'home_refreshes_after_commit'),
    ('app delete does not re-index surviving rows',
     'gw_screen_network.inc',
     '            g_app_row_ids[i] = g_app_row_ids[i + 1];',
     '',
     'delete_reindexes_rows'),
    ('scan timeout never fires',
     'gw_status.inc',
     'now - g_wifi_scan_started_ms >= 15000',
     'now - g_wifi_scan_started_ms >= 150000000',
     'scan_timeout_clears_busy'),
    ('network selection follows row position',
     'gw_screen_notes.inc',
     '    if (g_net_selected_ssid[0]) selected_ssid = g_net_selected_ssid;',
     '    if (g_net_ap_count > 1) selected_ssid = g_net_aps[1].ssid;',
     'selection_follows_ssid'),
]


def run_suite(tree: Path):
    env = dict(os.environ, HOSTCC=os.environ.get('HOSTCC', 'gcc'),
               LVGL_DIR=str(LVGL))
    build = subprocess.run(['make', 'host-sim'], cwd=tree, env=env,
                           capture_output=True, text=True)
    if build.returncode != 0:
        raise SystemExit(f'build failed in {tree}:\n{build.stdout}\n{build.stderr}')
    with tempfile.TemporaryDirectory() as out:
        result = subprocess.run([str(tree / 'host-sim'), out], capture_output=True,
                                text=True)
    fails = set(re.findall(r'FAIL \S+\s+(\S+)', result.stdout))
    return result.returncode, fails, result.stdout


def main():
    if not LVGL.is_dir():
        raise SystemExit(f'LVGL missing at {LVGL}')
    with tempfile.TemporaryDirectory() as tmp:
        repo = Path(tmp) / 'repo'
        (repo / 'os').mkdir(parents=True)
        clean = repo / 'os' / 'ui'
        shutil.copytree(UI, clean, ignore=shutil.ignore_patterns(
            'host-sim', 'gw-shell', 'gw-shell-debug', 'test-epdc-policy',
            '*.o', '__pycache__'))
        shutil.copytree(ROOT / 'os/system', repo / 'os' / 'system')
        rc, fails, _ = run_suite(clean)
        if rc != 0 or fails:
            raise SystemExit(f'unmodified suite is not green: fails={sorted(fails)}')
        print('clean suite: PASS')

        for name, filename, old, new, expected in MUTATIONS:
            tree = repo / 'os' / re.sub(r'\W+', '-', name)
            shutil.copytree(clean, tree)
            path = tree / filename
            text = path.read_text()
            if old not in text:
                raise SystemExit(f'mutation anchor missing for {name}')
            path.write_text(text.replace(old, new, 1))
            rc, fails, output = run_suite(tree)
            if expected not in fails:
                raise SystemExit(
                    f'{name}: expected {expected!r} to fail, got {sorted(fails)}\n{output}')
            print(f'{name}: FAILS as expected ({expected})')
    print('pre-fix regression evidence: PASS')


if __name__ == '__main__':
    sys.exit(main())
