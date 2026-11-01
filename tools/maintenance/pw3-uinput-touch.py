#!/usr/bin/env python3
"""Create a temporary uinput touch device and inject taps on the PW3.

Test-only helper for validating the real evdev/poll input path.  It does not
touch the physical touch controller.  Requires root and an existing uinput
misc device (the PW3 kernel has uinput built in at misc minor 223).  The shell
must be a diagnostic build compiled with -DGW_UI_TOUCH_DEV_OVERRIDE=1 and
started with GW_UI_TOUCH_DEV pointing at the event node.

Usage (on the device):
    python3 /tmp/pw3-uinput-touch.py --taps 10 --interval 3 --x 408 --y 864
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
UI_SET_ABSBIT = 0x40045567
UI_DEV_CREATE = 0x5501
UI_DEV_DESTROY = 0x5502

EV_SYN = 0
EV_KEY = 1
EV_ABS = 3
SYN_REPORT = 0
BTN_TOUCH = 0x14A
ABS_X = 0
ABS_Y = 1
ABS_MT_POSITION_X = 0x35
ABS_MT_POSITION_Y = 0x36
ABS_MT_TRACKING_ID = 0x39
ABS_CNT = 64


def make_uinput() -> int:
    if not os.path.exists("/dev/uinput"):
        os.mknod("/dev/uinput", 0o600 | 0o020000, os.makedev(10, 223))
    fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
    for event in (EV_SYN, EV_KEY, EV_ABS):
        fcntl.ioctl(fd, UI_SET_EVBIT, event)
    fcntl.ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH)
    for axis in (ABS_X, ABS_Y, ABS_MT_POSITION_X, ABS_MT_POSITION_Y,
                 ABS_MT_TRACKING_ID):
        fcntl.ioctl(fd, UI_SET_ABSBIT, axis)

    name = b"gw-test-touch".ljust(80, b"\0")
    ident = struct.pack("HHHH", 0x03, 0x1234, 0x5678, 1)
    effects = struct.pack("I", 0)
    maximum = [0] * ABS_CNT
    minimum = [0] * ABS_CNT
    fuzz = [0] * ABS_CNT
    flat = [0] * ABS_CNT
    maximum[ABS_X] = 1071
    maximum[ABS_Y] = 1447
    maximum[ABS_MT_POSITION_X] = 1071
    maximum[ABS_MT_POSITION_Y] = 1447
    maximum[ABS_MT_TRACKING_ID] = 65535
    payload = name + ident + effects
    for values in (maximum, minimum, fuzz, flat):
        payload += b"".join(struct.pack("i", value) for value in values)
    os.write(fd, payload)
    fcntl.ioctl(fd, UI_DEV_CREATE)
    return fd


def find_node() -> str | None:
    for sys_path in sorted(glob.glob("/sys/class/input/event*/device/name")):
        try:
            with open(sys_path) as stream:
                if stream.read().strip() == "gw-test-touch":
                    event = sys_path.split("/")[4]
                    return "/dev/input/" + event
        except OSError:
            pass
    return None


def emit(fd: int, ev_type: int, code: int, value: int) -> None:
    # 32-bit Linux input_event: two 32-bit timeval fields, u16, u16, s32.
    os.write(fd, struct.pack("IIHHi", 0, 0, ev_type, code, value))


def tap(fd: int, x: int, y: int, hold: float) -> None:
    emit(fd, EV_ABS, ABS_MT_TRACKING_ID, 1)
    emit(fd, EV_ABS, ABS_MT_POSITION_X, x)
    emit(fd, EV_ABS, ABS_MT_POSITION_Y, y)
    emit(fd, EV_ABS, ABS_X, x)
    emit(fd, EV_ABS, ABS_Y, y)
    emit(fd, EV_KEY, BTN_TOUCH, 1)
    emit(fd, EV_SYN, SYN_REPORT, 0)
    time.sleep(hold)
    emit(fd, EV_KEY, BTN_TOUCH, 0)
    emit(fd, EV_ABS, ABS_MT_TRACKING_ID, -1)
    emit(fd, EV_SYN, SYN_REPORT, 0)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--taps", type=int, default=10)
    parser.add_argument("--interval", type=float, default=3.0)
    parser.add_argument("--hold", type=float, default=0.08)
    parser.add_argument("--settle", type=float, default=8.0)
    parser.add_argument("--after", type=float, default=5.0)
    parser.add_argument("--x", type=int, default=408)
    parser.add_argument("--y", type=int, default=864)
    parser.add_argument("--node-file", default="/tmp/uinput-event")
    args = parser.parse_args()

    fd = make_uinput()
    node = None
    for _ in range(50):
        node = find_node()
        if node:
            break
        time.sleep(0.1)
    if not node:
        raise SystemExit("uinput event node did not appear")
    # uinput nodes appear after boot-time /dev/input creation; create the
    # character node explicitly when devtmpfs/udev did not.
    if not os.path.exists(node):
        with open("/sys/class/input/%s/dev" % node.rsplit("/", 1)[1]) as stream:
            major, minor = (int(part) for part in stream.read().strip().split(":"))
        os.mknod(node, 0o600 | 0o020000, os.makedev(major, minor))
    with open(args.node_file, "w") as stream:
        stream.write(node + "\n")
    print("READY", node, flush=True)
    time.sleep(args.settle)
    for index in range(args.taps):
        tap(fd, args.x, args.y, args.hold)
        print("TAP", index, flush=True)
        if index + 1 < args.taps:
            time.sleep(args.interval)
    time.sleep(args.after)
    fcntl.ioctl(fd, UI_DEV_DESTROY)
    os.close(fd)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
