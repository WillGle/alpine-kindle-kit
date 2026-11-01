#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Host hardening tests for destructive tooling.

Run with `python3 -O -m unittest discover -s tools/tests` to prove safety
decisions do not depend on assert statements.
"""
from __future__ import annotations

import io
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/lib"))

import pw3_common  # noqa: E402


def load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class SafetyTests(unittest.TestCase):
    def test_require_survives_optimisation(self):
        with self.assertRaises(ValueError):
            pw3_common.require(False, "blocked")
        pw3_common.require(True, "unused")

    def test_run_error_contains_command_and_output(self):
        result = subprocess.CompletedProcess(
            ["/bin/false"], 7, b"out-bytes", b"err-bytes")
        with mock.patch.object(subprocess, "run", return_value=result):
            with self.assertRaises(pw3_common.CommandError) as caught:
                pw3_common.run(["/bin/false"])
        text = str(caught.exception)
        self.assertIn("/bin/false", text)
        self.assertIn("out-bytes", text)
        self.assertIn("err-bytes", text)

    def test_write_atomic_leaves_no_partial(self):
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp) / "evidence.json"
            pw3_common.write_atomic(target, "first")
            pw3_common.write_atomic(target, "second")
            self.assertEqual(target.read_text(), "second")
            self.assertEqual(list(Path(tmp).glob("*.partial.*")), [])

    def test_rescue_unmounts_on_failure(self):
        module = load_module("kindle_linux", ROOT / "tools/maintenance/kindle-linux.py")
        rescue = object.__new__(module.Rescue)
        rescue.partition = "/dev/sdz1"
        rescue.mount = Path("/tmp/not-mounted")
        with mock.patch.object(subprocess, "run") as run:
            with self.assertRaises(RuntimeError):
                with rescue:
                    raise RuntimeError("injected failure")
        run.assert_any_call(["udisksctl", "unmount", "-b", "/dev/sdz1"],
                            capture_output=True)
        self.assertIsNone(rescue.mount)

    def test_pw3_build_rejects_escaping_symlink(self):
        module = load_module("pw3_build", ROOT / "tools/build/pw3-build.py")
        with tempfile.TemporaryDirectory() as tmp:
            archive = Path(tmp) / "root.tar"
            with tarfile.open(archive, "w") as tar:
                info = tarfile.TarInfo("escape")
                info.type = tarfile.SYMTYPE
                info.linkname = "../../etc/shadow"
                tar.addfile(info)
            with self.assertRaises(ValueError):
                module.unpack(archive, Path(tmp) / "tree")

    def test_gwapp_rejects_symlink_payload(self):
        gwapp = ROOT / "tools/build/gwapp.py"
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "source"
            source.mkdir()
            (source / "manifest").write_text(
                "id=test\nversion=1.0.0\nentrypoint=payload/run\n"
                "formats=txt\npermissions=display\narch=armv7\n"
                "profile=pw3-0kb-5.16.2.1.1\nmin_os=0.1\n")
            payload = source / "payload"
            payload.mkdir()
            (payload / "run").write_text("#!/bin/sh\n")
            (payload / "run").chmod(0o755)
            os.symlink("/etc/passwd", payload / "leak")
            key = Path(tmp) / "key.pem"
            key.write_text("not a key")
            result = subprocess.run(
                [sys.executable, "-O", str(gwapp), "build", "--source",
                 str(source), "--out", str(Path(tmp) / "test.gwapp"),
                 "--key", str(key)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("symlink rejected", result.stderr)

    def test_verify_package_rejects_traversal_under_O(self):
        verifier = ROOT / "tools/package/verify-package.py"
        with tempfile.TemporaryDirectory() as tmp:
            archive = Path(tmp) / "bad.tar.gz"
            with tarfile.open(archive, "w:gz") as tar:
                data = b"x"
                info = tarfile.TarInfo("../escape")
                info.size = 1
                tar.addfile(info, io.BytesIO(data))
            result = subprocess.run(
                [sys.executable, "-O", str(verifier), str(archive)],
                capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
