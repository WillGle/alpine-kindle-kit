#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fail closed on environment-specific paths and unconditional acceptance flags."""
from __future__ import annotations

from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SKIP_PREFIXES = ("docs/", "archive/", "dist/", "work/")
SKIP_SUFFIXES = (".json", ".md", ".png", ".ppm", ".log", ".sha256")


def tracked():
    result = subprocess.run(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"], cwd=ROOT, capture_output=True)
    for name in result.stdout.decode().split("\0"):
        if not name or name.startswith(SKIP_PREFIXES):
            continue
        if name == "tools/check_paths.py":
            continue
        if name.endswith(SKIP_SUFFIXES):
            continue
        path = ROOT / name
        if not path.is_file():
            continue
        try:
            yield name, path.read_text(errors="replace")
        except OSError:
            continue


def flags() -> int:
    failures = []
    pattern = re.compile(r"(?<![\w-])GW_[A-Z0-9_]+_ACCEPTED\s*=\s*1\b")
    for name, text in tracked():
        if name.endswith((".py", ".c", ".h", ".inc")):
            continue
        for number, line in enumerate(text.splitlines(), 1):
            if line.lstrip().startswith("#"):
                continue
            if pattern.search(line):
                failures.append(f"{name}:{number}: unconditional acceptance flag")
    for item in failures:
        print(item, file=sys.stderr)
    return 1 if failures else 0


def paths() -> int:
    failures = []
    pattern = re.compile(r"(/nix/store\b|/home/[A-Za-z0-9_.-]+/)")
    for name, text in tracked():
        for number, line in enumerate(text.splitlines(), 1):
            if pattern.search(line):
                failures.append(f"{name}:{number}: hardcoded host path")
    for item in failures:
        print(item, file=sys.stderr)
    return 1 if failures else 0


def main() -> int:
    if len(sys.argv) != 2 or sys.argv[1] not in ("flags", "paths"):
        print("usage: check_paths.py {flags|paths}", file=sys.stderr)
        return 2
    return flags() if sys.argv[1] == "flags" else paths()


if __name__ == "__main__":
    raise SystemExit(main())
