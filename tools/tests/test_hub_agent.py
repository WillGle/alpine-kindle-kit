#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Host integration tests for gw-agent.

These tests never touch a device, /dev/fb0 or the real /run socket.  A fake
hub serves one raw frame and records events; a fake shell socket receives
touch input.
"""
from __future__ import annotations

import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import threading
import time
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = Path(__file__).resolve().parents[2]
AGENT_SOURCE = ROOT / "os/system" / "gw-agent.c"
FRAME_BYTES = 1072 * 1448
FRAME = (bytes(range(256)) * (FRAME_BYTES // 256 + 1))[:FRAME_BYTES]


class HubState:
    def __init__(self) -> None:
        self.lock = threading.Lock()
        self.auth: list[str | None] = []
        self.events: list[dict] = []
        self.statuses: list[dict] = []
        self.no_content = False
        self.oversize = False
        self.bad_length: str | None = None
        self.header_pad_bytes: int = 0


class FakeHub(BaseHTTPRequestHandler):
    state: HubState

    def log_message(self, *args) -> None:  # noqa: D401 - keep test output quiet
        pass

    def do_GET(self) -> None:
        with self.state.lock:
            self.state.auth.append(self.headers.get("Authorization"))
        if self.path != "/v1/frame":
            self.send_error(404)
            return
        if self.state.no_content:
            self.send_response(204)
            self.end_headers()
            return
        body = FRAME
        length = len(body) + 1 if self.state.oversize else len(body)
        self.send_response(200)
        self.send_header("Content-Type", "image/x-gw-raw")
        if self.state.bad_length is not None:
            self.send_header("Content-Length", self.state.bad_length)
        else:
            self.send_header("Content-Length", str(length))
        self.send_header("X-GW-Seq", "7")
        self.send_header("X-GW-Mode", "full")
        self.send_header("X-GW-Rect", "-")
        if self.state.header_pad_bytes:
            self.send_header("X-GW-Pad", "x" * self.state.header_pad_bytes)
        self.end_headers()
        try:
            self.wfile.write(body)
            if self.state.oversize:
                self.wfile.write(b"x")
        except (BrokenPipeError, ConnectionResetError):
            # The agent rejects an oversized body/header before reading it.
            pass

    def do_POST(self) -> None:
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length)
        with self.state.lock:
            self.state.auth.append(self.headers.get("Authorization"))
            if self.path == "/v1/events":
                self.state.events.append(json.loads(body))
            elif self.path == "/v1/status":
                self.state.statuses.append(json.loads(body))
        self.send_response(200)
        self.send_header("Content-Length", "2")
        self.end_headers()
        self.wfile.write(b"{}")


class AgentTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.build = tempfile.TemporaryDirectory()
        cls.binary = Path(cls.build.name) / "gw-agent"
        subprocess.run(
            [os.environ.get("HOSTCC", "cc"), "-std=c99", "-Wall", "-Wextra",
             "-O2", "-I", str(ROOT / "os/system"), "-o", str(cls.binary),
             str(AGENT_SOURCE)],
            check=True,
        )

    @classmethod
    def tearDownClass(cls) -> None:
        cls.build.cleanup()

    def setUp(self) -> None:
        self.state = HubState()
        handler = type("Handler", (FakeHub,), {"state": self.state})
        self.server = ThreadingHTTPServer(("127.0.0.1", 0), handler)
        self.thread = threading.Thread(target=self.server.serve_forever,
                                       daemon=True)
        self.thread.start()
        self.tmp = tempfile.TemporaryDirectory()
        self.tmpdir = Path(self.tmp.name)
        self.config = self.tmpdir / "hub.conf"
        self.config.write_text(
            f"host=127.0.0.1\n"
            f"port={self.server.server_port}\n"
            "token=test-token\n"
            "device_id=pw3-test\n"
            "poll_ms=500\n"
        )
        self.config.chmod(0o600)
        self.frame_path = self.tmpdir / "frame.raw"

    def tearDown(self) -> None:
        self.server.shutdown()
        self.server.server_close()
        self.tmp.cleanup()

    def run_once(self, config: Path | None = None) -> subprocess.CompletedProcess:
        return subprocess.run(
            [str(self.binary), "--config", str(config or self.config),
             "--frame", str(self.frame_path), "--once", "--no-ui"],
            capture_output=True, text=True, timeout=20,
        )

    def test_fetches_frame_and_posts_status(self) -> None:
        result = self.run_once()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.frame_path.read_bytes(), FRAME)
        self.assertTrue(self.state.statuses)
        self.assertEqual(self.state.statuses[0]["device_id"], "pw3-test")
        self.assertTrue(all(a == "Bearer test-token" for a in self.state.auth))

    def test_no_content_is_not_an_error(self) -> None:
        self.state.no_content = True
        result = self.run_once()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(self.frame_path.exists())
        self.assertTrue(self.state.statuses)

    def test_oversize_body_is_rejected(self) -> None:
        self.state.oversize = True
        result = self.run_once()
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(self.frame_path.exists())

    def test_bad_content_length_is_rejected(self) -> None:
        for value in ("not-a-number", "-5", "999999999999"):
            with self.subTest(value=value):
                self.state.bad_length = value
                result = self.run_once()
                self.assertNotEqual(result.returncode, 0, result.stderr)
                self.assertFalse(self.frame_path.exists())
                self.state.bad_length = None

    def test_oversize_header_is_rejected(self) -> None:
        self.state.header_pad_bytes = 9000
        result = self.run_once()
        self.assertNotEqual(result.returncode, 0, result.stderr)
        self.assertFalse(self.frame_path.exists())

    def test_config_symlink_is_rejected(self) -> None:
        link = self.tmpdir / "link.conf"
        link.symlink_to(self.config)
        result = self.run_once(link)
        self.assertNotEqual(result.returncode, 0)

    def test_touch_is_forwarded_to_hub(self) -> None:
        self.state.no_content = True
        sock_path = self.tmpdir / "gw-hub.sock"
        listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        listener.bind(str(sock_path))
        listener.listen(1)
        listener.settimeout(5)
        proc = subprocess.Popen(
            [str(self.binary), "--config", str(self.config),
             "--socket", str(sock_path), "--frame", str(self.frame_path)],
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
        )
        try:
            conn, _ = listener.accept()
            conn.sendall(b"TOUCH down 32768 40000\n")
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline and not self.state.events:
                time.sleep(0.05)
            self.assertTrue(self.state.events, "no event reached the hub")
            self.assertEqual(self.state.events[0]["type"], "touch")
            self.assertEqual(self.state.events[0]["action"], "down")
            self.assertEqual(self.state.events[0]["x"], 32768)
            self.assertEqual(self.state.events[0]["y"], 40000)
            conn.close()
        finally:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait(timeout=5)
            if proc.stderr is not None:
                proc.stderr.close()
            listener.close()


if __name__ == "__main__":
    unittest.main()
