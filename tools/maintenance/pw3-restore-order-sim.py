#!/usr/bin/env python3
"""Offline crash-point model of the PW3 single-slot root replacement.

This does not touch a device. It mirrors the rename order in
tools/maintenance/kindle-linux.py deploy() and the manual rollback order used in
Phase 4B, then evaluates whether the existing bootstrap can still boot or
whether the USB rescue path is required.

The bootstrap has no A/B selector: both boot.once and boot.enabled select the
same fixed path, /gleworks-alpine/system.ext3. This model therefore records
single-slot crash safety, not true A/B redundancy.
"""
from __future__ import annotations

import argparse
import json
import shutil
import tempfile
from pathlib import Path

TOKEN = b"gleworks-alpine-v1\n"
OLD = b"OLD-PRODUCTION-IMAGE\n"
NEW = b"CANDIDATE-IMAGE\n"


def snapshot(root: Path) -> dict:
    files = {p.name: p.read_bytes() for p in root.iterdir() if p.is_file()}
    system = files.get("system.ext3")
    enabled = files.get("boot.enabled")
    backup_names = sorted(n for n in files if n.startswith("system-") and n.endswith(".ext3"))
    return {
        "system_present": system is not None,
        "system_is_old": system == OLD,
        "system_is_candidate": system == NEW,
        "boot_enabled_valid": enabled == TOKEN,
        "boot_enabled_present": enabled is not None,
        "preserved_images": backup_names,
        "partial_present": (root / "system.native-kit.partial").exists(),
        "old_image_available": any(files[n] == OLD for n in backup_names),
        "candidate_image_available": any(files[n] == NEW for n in backup_names),
    }


def evaluate(state: dict) -> dict:
    bootable = bool(state["system_present"] and state["boot_enabled_valid"])
    rescue = not bootable
    recoverable = (
        state["system_is_old"]
        or state["system_is_candidate"]
        or state["old_image_available"]
        or state["candidate_image_available"]
    )
    return {
        **state,
        "bootstrap_selects_alpine": bootable,
        "requires_usb_rescue": rescue,
        "complete_image_recoverable": bool(recoverable),
    }


def write(path: Path, data: bytes) -> None:
    path.write_bytes(data)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    root = Path(tempfile.mkdtemp(prefix="pw3-restore-order-"))
    cases: dict[str, dict] = {}
    try:
        # Known-good state. Production uses system.ext3 plus boot.enabled.
        write(root / "system.ext3", OLD)
        write(root / "boot.enabled", TOKEN)
        cases["0_known_good"] = evaluate(snapshot(root))

        # Rescue entry removes the normal boot token before any replacement.
        (root / "boot.enabled").unlink()
        cases["1_rescue_no_token"] = evaluate(snapshot(root))

        # deploy(): copy to a new partial and fsync it before touching current.
        write(root / "system.native-kit.partial", NEW)
        cases["2_after_candidate_partial"] = evaluate(snapshot(root))

        # deploy(): current is renamed to a never-overwritten backup.
        (root / "system.ext3").rename(root / "system-restore-drill.ext3")
        cases["3_after_old_renamed_backup"] = evaluate(snapshot(root))

        # deploy(): verified partial becomes the only active root.
        (root / "system.native-kit.partial").rename(root / "system.ext3")
        cases["4_after_candidate_installed"] = evaluate(snapshot(root))

        # deploy(): normal boot token is written only after both images exist.
        write(root / "boot.enabled", TOKEN)
        cases["5_after_boot_token"] = evaluate(snapshot(root))

        # Rollback begins in rescue: active candidate is preserved, backup returns.
        (root / "boot.enabled").unlink()
        (root / "system.ext3").rename(root / "system-restoration-candidate.ext3")
        (root / "system-restore-drill.ext3").rename(root / "system.ext3")
        cases["6_rollback_image_restored"] = evaluate(snapshot(root))

        # The rescue token is restored last and verified before safe eject.
        write(root / "boot.enabled", TOKEN)
        cases["7_rollback_token_restored"] = evaluate(snapshot(root))

        for name, case in cases.items():
            if not case["complete_image_recoverable"]:
                raise SystemExit(f"{name}: no complete image remains")
            if not case["system_present"] and not case["old_image_available"] and not case["candidate_image_available"]:
                raise SystemExit(f"{name}: active slot is empty and no preserved image exists")
            if name in {"1_rescue_no_token", "3_after_old_renamed_backup", "4_after_candidate_installed", "6_rollback_image_restored"}:
                if not case["requires_usb_rescue"]:
                    raise SystemExit(f"{name}: expected rescue because boot token was absent")
            if name in {"5_after_boot_token", "7_rollback_token_restored"} and not case["bootstrap_selects_alpine"]:
                raise SystemExit(f"{name}: expected a bootable selected root")

        result = {
            "model": "PW3 single-slot system.ext3 replacement and manual rollback",
            "selection": {
                "boot.once": "one-shot request, same fixed /gleworks-alpine/system.ext3 path",
                "boot.enabled": "persistent request, same fixed /gleworks-alpine/system.ext3 path",
                "true_ab_selection": False,
                "symlink_workaround": "rejected by bootstrap O_NOFOLLOW",
                "hash_manifest_in_bootstrap": False,
            },
            "cases": cases,
        }
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps(result, indent=2))
    finally:
        shutil.rmtree(root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
