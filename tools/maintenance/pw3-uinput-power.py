#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Create a temporary uinput KEY_POWER device and inject presses on the PW3.

Test-only helper for the physical evdev path.  The production service keeps
owning the real max77696 button; this device is selected explicitly with
GW_POWER_BUTTON_DEV=<event node> for an acceptance run.

Usage (on the device):
    python3 /tmp/pw3-uinput-power.py --serve \
        --node-file /run/gw-test-power.node --command-file /run/gw-test-power.cmd
Then: echo press > /run/gw-test-power.cmd
"""
from __future__ import annotations

import argparse
import fcntl
import glob
import os
import struct
import time

UI_SET_EVBIT = 0x40045564
UI_SET_KEYBIT = 0x40045565
UI_DEV_CREATE = 0x5501
UI_DEV_DESTROY = 0x5502

EV_SYN = 0
EV_KEY = 1
SYN_REPORT = 0
KEY_POWER = 116

INPUT_EVENT = struct.Struct("llHHi")   # 32-bit timeval + type/code/value


def make_uinput() -> int:
    if not os.path.exists("/dev/uinput"):
        os.mknod("/dev/uinput", 0o600 | 0o020000, os.makedev(10, 223))
    fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
    fcntl.ioctl(fd, UI_SET_EVBIT, EV_SYN)
    fcntl.ioctl(fd, UI_SET_EVBIT, EV_KEY)
    fcntl.ioctl(fd, UI_SET_KEYBIT, KEY_POWER)
    name = b"gw-test-power".ljust(80, b"\0")
    ident = struct.pack("HHHH", 0x03, 0x1234, 0x5679, 1)
    effects = struct.pack("I", 0)
    # uinput_user_dev: name, input_id, ff_effects_max, absmax/min/fuzz/flat.
    payload = name + ident + effects
    payload += b"\0" * (4 * 4 * 64)
    os.write(fd, payload)
    fcntl.ioctl(fd, UI_DEV_CREATE)
    return fd


def find_node() -> str | None:
    for path in sorted(glob.glob("/sys/class/input/event*/device/name")):
        try:
            with open(path) as stream:
                if stream.read().strip() == "gw-test-power":
                    return "/dev/input/" + path.split("/")[4]
        except OSError:
            pass
    return None


def emit(fd: int, value: int) -> None:
    now = time.time()
    sec, usec = int(now), int((now - int(now)) * 1_000_000)
    os.write(fd, INPUT_EVENT.pack(sec, usec, EV_KEY, KEY_POWER, value))
    os.write(fd, INPUT_EVENT.pack(sec, usec, EV_SYN, SYN_REPORT, 0))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--serve", action="store_true")
    parser.add_argument("--node-file", default="/run/gw-test-power.node")
    parser.add_argument("--command-file", default="/run/gw-test-power.cmd")
    parser.add_argument("--hold-ms", type=int, default=120)
    args = parser.parse_args()
    if os.geteuid() != 0:
        raise SystemExit("root required")
    fd = make_uinput()
    node = None
    for _ in range(50):
        node = find_node()
        if node:
            break
        time.sleep(0.1)
    if not node:
        raise SystemExit("uinput node did not appear")
    # uinput events appear after the boot-time /dev/input node creation, so
    # create the character node explicitly when devtmpfs/udev did not.
    if not os.path.exists(node):
        with open("/sys/class/input/%s/dev" % node.rsplit("/", 1)[1]) as stream:
            major, minor = (int(part) for part in stream.read().strip().split(":"))
        os.mknod(node, 0o600 | 0o020000, os.makedev(major, minor))
    with open(args.node_file, "w") as stream:
        stream.write(node + "\n")
    try:
        if not args.serve:
            emit(fd, 1)
            time.sleep(args.hold_ms / 1000)
            emit(fd, 0)
            return 0
        while True:
            try:
                with open(args.command_file) as stream:
                    command = stream.read().strip()
            except OSError:
                command = ""
            if command:
                open(args.command_file, "w").close()
                if command == "press":
                    emit(fd, 1)
                    time.sleep(args.hold_ms / 1000)
                    emit(fd, 0)
                elif command == "down":
                    emit(fd, 1)
                elif command == "up":
                    emit(fd, 0)
                elif command == "quit":
                    break
            time.sleep(0.1)
    finally:
        try:
            fcntl.ioctl(fd, UI_DEV_DESTROY)
        except OSError:
            pass
        os.close(fd)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
