#!/usr/bin/env python3
"""Offline PW3 bootstrap image-selection simulations on disposable copies.

Never opens the source for writing. Mirrors bootstrap.c image_clean():
- ext3 magic at byte 1024+56
- filesystem revision 1
- INCOMPAT_RECOVER clear
- image size >= 64 MiB
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def image_clean(path: Path) -> tuple[bool, str]:
    if not path.is_file() or path.is_symlink():
        return False, "missing-or-not-regular"
    if path.stat().st_size < 64 * 1024 * 1024 or path.stat().st_size > 1024**3:
        return False, "bad-size"
    with path.open("rb") as f:
        f.seek(1024)
        sb = f.read(104)
    if len(sb) != 104:
        return False, "short-superblock"
    if sb[56:60] != b"\x53\xef\x01\x00":
        return False, "bad-magic-or-revision"
    if sb[96] & 4:
        return False, "needs-journal-recovery"
    return True, "clean"


def copy_reflink(src: Path, dst: Path) -> None:
    if dst.exists():
        raise SystemExit(f"refusing existing {dst}")
    subprocess.run(["cp", "--reflink=auto", str(src), str(dst)], check=True)


def fsck(path: Path) -> dict:
    result = subprocess.run(["e2fsck", "-fn", str(path)], capture_output=True, text=True)
    tail = [line for line in result.stdout.splitlines() if line.strip()][-6:]
    return {"rc": result.returncode, "tail": tail}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--outdir", type=Path, required=True)
    args = parser.parse_args()
    src = args.source.resolve()
    if not src.is_file():
        raise SystemExit("source must be a regular file")
    out = args.outdir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    results = {"source": str(src), "sha256": digest(src), "cases": {}}

    results["cases"]["clean"] = {
        "decision": image_clean(src),
        "fsck": fsck(src) if shutil.which("e2fsck") else "unavailable",
    }

    dirty = out / "dirty-journal.ext3"
    copy_reflink(src, dirty)
    with dirty.open("r+b") as f:
        f.seek(1024 + 96)
        value = f.read(1)[0]
        f.seek(1024 + 96)
        f.write(bytes([value | 4]))
        f.flush()
        os.fsync(f.fileno())
    results["cases"]["dirty_journal"] = {
        "decision": image_clean(dirty),
        "fsck": fsck(dirty),
    }

    bad = out / "bad-metadata.ext3"
    copy_reflink(src, bad)
    with bad.open("r+b") as f:
        f.seek(1024 + 56)
        f.write(b"\x00\x00\x00\x00")
        f.flush()
        os.fsync(f.fileno())
    results["cases"]["invalid_metadata"] = {
        "decision": image_clean(bad),
        "fsck": fsck(bad),
    }

    truncated = out / "truncated-update.ext3"
    copy_reflink(src, truncated)
    with truncated.open("r+b") as f:
        f.truncate(16 * 1024 * 1024)
        f.flush()
        os.fsync(f.fileno())
    results["cases"]["truncated_update"] = {"decision": image_clean(truncated)}

    missing = out / "missing.ext3"
    results["cases"]["missing_image"] = {"decision": image_clean(missing)}

    restored = out / "restored-copy.ext3"
    copy_reflink(src, restored)
    results["cases"]["restore_copy"] = {
        "decision": image_clean(restored),
        "sha256_matches": digest(restored) == digest(src),
        "fsck": fsck(restored),
    }

    results["cases"]["read_only_root"] = {
        "decision": "code-inspection",
        "expected": "failure before early startup completes can leave boot.repairing and reach rescue; after early startup cancels Guardian and clears boot.repairing, errors=remount-ro does not reboot or force rescue",
    }
    args_json = out / "simulation.json"
    args_json.write_text(json.dumps(results, indent=2) + "\n")
    print(json.dumps(results, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
