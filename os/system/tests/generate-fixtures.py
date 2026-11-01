#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Regenerate signed install fixtures for test-apps-install.

Requires the OpenSSL CLI.  The committed outputs are what the host test uses;
this script exists so the fixtures can be audited and rebuilt.
"""
import io
import os
import pathlib
import subprocess
import tarfile
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
KEYS = HERE / "keys"
FIXTURES = HERE / "fixtures"

MANIFEST = """id={id}
version={version}
entrypoint=payload/hello.sh
formats=txt
permissions=display
arch=armv7
profile={profile}
min_os=0.1
description={description}
"""


def manifest(app_id, version="1.0.0", profile="pw3-0kb-5.16.2.1.1",
             description="fixture"):
    return MANIFEST.format(id=app_id, version=version, profile=profile,
                           description=description).encode()


def add_bytes(tar, name, data, mode=0o755):
    info = tarfile.TarInfo(name)
    info.size = len(data)
    info.mode = mode
    tar.addfile(info, io.BytesIO(data))


def add_link(tar, name, target, kind):
    info = tarfile.TarInfo(name)
    info.type = kind
    info.linkname = target
    info.mode = 0o755
    tar.addfile(info)


def add_zeros(tar, name, size):
    class Zeros(io.RawIOBase):
        def __init__(self):
            self.left = size

        def readinto(self, b):
            n = min(len(b), self.left)
            b[:n] = b"\0" * n
            self.left -= n
            return n

        def readable(self):
            return True

    info = tarfile.TarInfo(name)
    info.size = size
    info.mode = 0o644
    tar.addfile(info, Zeros())


def build(path, add):
    with tarfile.open(path, "w:gz") as tar:
        add(tar)


def main():
    KEYS.mkdir(parents=True, exist_ok=True)
    FIXTURES.mkdir(parents=True, exist_ok=True)
    if not (KEYS / "test-release.key").exists():
        subprocess.run(["openssl", "genpkey", "-algorithm", "RSA",
                        "-pkeyopt", "rsa_keygen_bits:2048", "-out",
                        str(KEYS / "test-release.key")], check=True)
        subprocess.run(["openssl", "pkey", "-in", str(KEYS / "test-release.key"),
                        "-pubout", "-out", str(KEYS / "test-release.pem")],
                       check=True)
    if not (KEYS / "other-release.key").exists():
        subprocess.run(["openssl", "genpkey", "-algorithm", "RSA",
                        "-pkeyopt", "rsa_keygen_bits:2048", "-out",
                        str(KEYS / "other-release.key")], check=True)

    def valid(tar, app_id="valid", description="fixture"):
        add_bytes(tar, "manifest", manifest(app_id, description=description),
                  mode=0o644)
        add_bytes(tar, "payload/hello.sh",
                  ("#!/bin/sh\necho %s\n" % app_id).encode())

    def setuid(tar):
        valid(tar, "setuid")
        add_bytes(tar, "payload/su", b"#!/bin/sh\n", mode=0o4755)

    def hardlink(tar):
        valid(tar, "hardlink")
        add_bytes(tar, "payload/one", b"x")
        add_link(tar, "payload/two", "payload/one", tarfile.LNKTYPE)

    def symlink(tar):
        valid(tar, "symlink")
        add_link(tar, "payload/link", "/etc/shadow", tarfile.SYMTYPE)

    def duplicate(tar):
        add_bytes(tar, "manifest", manifest("duplicate"), mode=0o644)
        add_bytes(tar, "payload/hello.sh", b"#!/bin/sh\necho first\n")
        add_bytes(tar, "payload/hello.sh", b"#!/bin/sh\necho second\n")

    def oversized(tar):
        valid(tar, "oversized")
        add_zeros(tar, "payload/bomb.bin", 300 * 1024 * 1024)

    def wrong_profile(tar):
        add_bytes(tar, "manifest",
                  manifest("wrongprofile", profile="pw3-other-5.16.2.1.1"),
                  mode=0o644)
        add_bytes(tar, "payload/hello.sh", b"#!/bin/sh\necho wrongprofile\n")

    def version_overflow(tar):
        add_bytes(tar, "manifest",
                  manifest("overflow", version="99999999999999999999.1"),
                  mode=0o644)
        add_bytes(tar, "payload/hello.sh", b"#!/bin/sh\necho overflow\n")

    specs = {
        "valid.gwapp": valid,
        "swapped.gwapp": lambda tar: valid(tar, "swapped", "original"),
        "setuid.gwapp": setuid,
        "hardlink.gwapp": hardlink,
        "symlink.gwapp": symlink,
        "duplicate.gwapp": duplicate,
        "oversized.gwapp": oversized,
        "gzip-bomb.gwapp": oversized,
        "wrong-profile.gwapp": wrong_profile,
        "version-overflow.gwapp": version_overflow,
    }
    for name, add in specs.items():
        build(FIXTURES / name, add)

    # Same valid package, signed by a key that is not the trust root.
    with tempfile.NamedTemporaryFile(suffix=".gwapp", delete=False) as tmp:
        other = pathlib.Path(tmp.name)
    build(other, valid)
    for name in specs:
        source = FIXTURES / name
        key = "other-release.key" if name == "wrong-key.gwapp" else "test-release.key"
        subprocess.run(["openssl", "dgst", "-sha256", "-sign", str(KEYS / key),
                        "-out", str(source) + ".sig", str(source)], check=True)
    (FIXTURES / "wrong-key.gwapp").write_bytes(other.read_bytes())
    subprocess.run(["openssl", "dgst", "-sha256", "-sign",
                    str(KEYS / "other-release.key"), "-out",
                    str(FIXTURES / "wrong-key.gwapp") + ".sig",
                    str(FIXTURES / "wrong-key.gwapp")], check=True)
    other.unlink()


if __name__ == "__main__":
    main()
