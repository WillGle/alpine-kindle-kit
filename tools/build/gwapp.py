#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build and sign a GleWorks application bundle.

A bundle is a deterministic gzip tar containing `manifest` and the payload
tree declared by that manifest.  A detached RSA/SHA-256 signature is written
next to the bundle as <name>.gwapp.sig and verified on the device against
os/system/app-keys/release.pem.

Usage:
  gwapp.py keygen --private work/private/app-keys/release.key \
                  --public os/system/app-keys/release.pem
  gwapp.py build --source APP_DIR --out NAME.gwapp \
                 --key work/private/app-keys/release.key
"""
from __future__ import annotations

import argparse
import gzip
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile

REPO = Path(__file__).resolve().parents[2]


def find_openssl() -> str:
    value = os.environ.get("OPENSSL")
    if value:
        return value
    found = shutil.which("openssl")
    if found:
        return found
    raise SystemExit("openssl not found; set OPENSSL=/path/to/openssl")


def keygen(args: argparse.Namespace) -> None:
    os.umask(0o077)
    openssl = find_openssl()
    private = Path(args.private)
    public = Path(args.public)
    private.parent.mkdir(parents=True, exist_ok=True)
    if private.exists():
        raise SystemExit(f"refusing to overwrite existing private key: {private}")
    if public.exists():
        raise SystemExit(f"refusing to overwrite existing public key: {public}")
    subprocess.run([openssl, "genpkey", "-algorithm", "RSA",
                    "-pkeyopt", "rsa_keygen_bits:2048",
                    "-out", str(private)], check=True)
    private.chmod(0o600)
    public.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([openssl, "pkey", "-in", str(private), "-pubout",
                    "-out", str(public)], check=True)
    public.chmod(0o644)
    print(f"private: {private}\npublic:  {public}")


def safe_member(path: Path, root: Path) -> bool:
    rel = path.relative_to(root)
    parts = rel.parts
    if not parts or any(part in ("", ".", "..") for part in parts):
        return False
    for part in parts:
        if not all(c.isalnum() or c in "._+-" for c in part):
            return False
    return True


def read_manifest(path: Path) -> dict[str, str]:
    fields: dict[str, str] = {}
    for line in path.read_text().splitlines():
        key, separator, value = line.partition("=")
        if not separator or not key or key in fields:
            raise SystemExit(f"invalid manifest line: {line!r}")
        if any(ord(c) < 32 or ord(c) > 126 for c in line):
            raise SystemExit(f"manifest contains control characters: {line!r}")
        fields[key] = value
    required = {"id", "version", "entrypoint", "arch", "profile", "min_os",
                "formats", "permissions"}
    if not required.issubset(fields):
        raise SystemExit("manifest is missing required fields: "
                         + ", ".join(sorted(required - set(fields))))
    return fields


def build(args: argparse.Namespace) -> None:
    os.umask(0o077)
    source = Path(args.source).resolve()
    out = Path(args.out).resolve()
    key = Path(args.key).resolve()
    manifest = source / "manifest"
    if not manifest.is_file():
        raise SystemExit("source must contain a manifest file")
    if out.exists():
        raise SystemExit(f"refusing to overwrite {out}")
    if not key.is_file():
        raise SystemExit(f"missing signing key: {key}")

    # Never follow or package symlinks: a link can point outside the source
    # tree and change after validation.
    files = []
    for root, directories, names in os.walk(source, followlinks=False):
        root_path = Path(root)
        for name in list(directories):
            if (root_path / name).is_symlink():
                raise SystemExit(f"symlink rejected in bundle: {root_path / name}")
        for name in names:
            path = root_path / name
            if path.is_symlink():
                raise SystemExit(f"symlink rejected in bundle: {path}")
            if path.is_file():
                files.append(path)
    files.sort()
    for path in files:
        if not safe_member(path, source):
            raise SystemExit(f"unsafe bundle member: {path}")

    manifest_fields = read_manifest(manifest)
    entrypoint = manifest_fields["entrypoint"]
    declared = source / entrypoint
    if not safe_member(declared, source):
        raise SystemExit(f"unsafe manifest entrypoint: {entrypoint}")
    if not declared.is_file() or declared.is_symlink():
        raise SystemExit(f"manifest entrypoint missing: {entrypoint}")
    if declared.relative_to(source) not in [p.relative_to(source) for p in files]:
        raise SystemExit(f"manifest entrypoint is not in the payload: {entrypoint}")

    # New agent packages opt into a mandatory host check before any output is
    # signed. Legacy packages keep their existing, device-verified path.
    if (source / 'compatibility.json').exists() or args.rootfs:
        if not args.rootfs:
            raise SystemExit('compatibility.json requires --rootfs for the target userspace gate')
        from app_compat import check
        report = check(source, Path(args.rootfs), manifest_fields, args.os_version)
        if not report['static_passed']:
            print(json.dumps(report, indent=2), file=sys.stderr)
            raise SystemExit('app compatibility gate blocked signing')

    out.parent.mkdir(parents=True, exist_ok=True)
    # gzip mtime=0 and a fixed tar order/metadata make the signature stable for
    # an unchanged source tree.  The device verifies the exact bytes.
    with out.open("xb") as raw, gzip.GzipFile(fileobj=raw, mode="wb", mtime=0) as gz:
        with tarfile.open(fileobj=gz, mode="w", format=tarfile.GNU_FORMAT) as tar:
            for path in files:
                rel = path.relative_to(source).as_posix()
                data = path.read_bytes()
                info = tarfile.TarInfo(rel)
                info.size = len(data)
                info.mode = 0o755 if os.access(path, os.X_OK) else 0o644
                info.uid = info.gid = 0
                info.uname = info.gname = "root"
                info.mtime = 0
                tar.addfile(info, io.BytesIO(data))

    if args.rootfs and out.stat().st_size > 128 * 1024 * 1024:
        raise SystemExit('compressed package exceeds 128 MiB; unsigned output retained for inspection')
    signature = Path(str(out) + ".sig")
    subprocess.run([find_openssl(), "dgst", "-sha256", "-sign", str(key),
                    "-out", str(signature), str(out)], check=True)
    signature.chmod(0o644)
    print(f"bundle:    {out}\nsignature: {signature}")


def compatibility(args: argparse.Namespace) -> None:
    from app_compat import check
    source = Path(args.source)
    try:
        manifest = read_manifest(source / 'manifest')
    except (OSError, UnicodeError, SystemExit) as exc:
        print(json.dumps({'schema': 1, 'status': 'blocked', 'static_passed': False,
                          'errors': [{'code': 'manifest-invalid', 'detail': str(exc)}]}))
        raise SystemExit(1)
    report = check(source, Path(args.rootfs), manifest, args.os_version)
    print(json.dumps(report, indent=2))
    if not report['static_passed']:
        raise SystemExit(1)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("keygen")
    p.add_argument("--private", default=str(REPO / "work/private/app-keys/release.key"))
    p.add_argument("--public", default=str(REPO / "os/system/app-keys/release.pem"))
    p.set_defaults(func=keygen)
    p = sub.add_parser("build")
    p.add_argument("--source", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--key", default=str(REPO / "work/private/app-keys/release.key"))
    p.add_argument("--rootfs", help="Extracted target userspace; required for compatibility.json packages")
    p.add_argument("--os-version", default="0.1")
    p.set_defaults(func=build)
    p = sub.add_parser("check", help="Inspect app/target dependencies without running app code")
    p.add_argument("--source", required=True)
    p.add_argument("--rootfs", required=True)
    p.add_argument("--os-version", default="0.1")
    p.set_defaults(func=compatibility)
    args = parser.parse_args()
    args.func(args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
