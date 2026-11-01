#!/usr/bin/env python3
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
    candidates = sorted(Path("/nix/store").glob("*openssl-*-bin/bin/openssl"))
    if candidates:
        return str(candidates[-1])
    raise SystemExit("openssl not found; set OPENSSL=/path/to/openssl")


def keygen(args: argparse.Namespace) -> None:
    openssl = find_openssl()
    private = Path(args.private)
    public = Path(args.public)
    private.parent.mkdir(parents=True, exist_ok=True)
    if private.exists():
        raise SystemExit(f"refusing to overwrite existing private key: {private}")
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


def build(args: argparse.Namespace) -> None:
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

    files = sorted(p for p in source.rglob("*") if p.is_file())
    for path in files:
        if not safe_member(path, source):
            raise SystemExit(f"unsafe bundle member: {path}")

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

    signature = Path(str(out) + ".sig")
    subprocess.run([find_openssl(), "dgst", "-sha256", "-sign", str(key),
                    "-out", str(signature), str(out)], check=True)
    signature.chmod(0o644)
    print(f"bundle:    {out}\nsignature: {signature}")


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
    p.set_defaults(func=build)
    args = parser.parse_args()
    args.func(args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
