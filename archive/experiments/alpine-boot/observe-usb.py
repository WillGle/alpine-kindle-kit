#!/usr/bin/env python3
"""Record a live USB challenge response from the minimum Alpine PID1 userspace."""
import argparse
import json
import os
from pathlib import Path
import re
import secrets
import select
import termios
import time
import tty

parser = argparse.ArgumentParser()
parser.add_argument("--device", required=True, help="USB ACM tty for the Kindle diagnostic gadget")
parser.add_argument("--seconds", type=int, default=180)
parser.add_argument("--output", required=True)
args = parser.parse_args()
if args.seconds < 1 or args.seconds > 600:
    parser.error("seconds must be 1..600")
challenge = secrets.token_hex(16)
deadline = time.monotonic() + args.seconds
output = Path(args.output)
if output.exists():
    parser.error("retain existing observation; choose a new output path")
buffer = b""
matched = None
fd = None
sent_at = 0.0
errors = []
try:
    while time.monotonic() < deadline and matched is None:
        if fd is None:
            try:
                fd = os.open(args.device, os.O_RDWR | os.O_NONBLOCK | os.O_NOCTTY)
                tty.setraw(fd)
                sent_at = 0.0
            except (OSError, termios.error) as error:
                if fd is not None:
                    os.close(fd)
                    fd = None
                errors.append(str(error))
                time.sleep(0.2)
                continue
        try:
            if time.monotonic() - sent_at > 1.0:
                os.write(fd, challenge.encode() + b"\n")
                sent_at = time.monotonic()
            readable, _, _ = select.select([fd], [], [], 0.2)
            if not readable:
                continue
            data = os.read(fd, 4096)
            if not data:
                raise OSError("USB tty disconnected")
            buffer = (buffer + data)[-65536:]
            expression = rb"GLEWORKS_ALPINE_BOOT release=3\.24\.3 kernel=6\.18\.0-gleworks-alpine-probe pid1_exe=/bin/busybox challenge=" + challenge.encode() + rb"\r?\n"
            response = re.search(expression, buffer)
            if response:
                matched = response.group(0).decode().strip()
        except OSError as error:
            errors.append(str(error))
            os.close(fd)
            fd = None
finally:
    if fd is not None:
        os.close(fd)
    result = {"alpine_boot_observed": matched is not None,
              "device": args.device, "challenge": challenge, "response": matched,
              "usb_tail": buffer.decode(errors="replace"), "recent_errors": errors[-10:]}
    with output.open("x") as stream:
        json.dump(result, stream, indent=2)
        stream.write("\n")
print("Minimum Alpine userspace observed." if matched else "No boot confirmation observed.")
raise SystemExit(0 if matched else 1)
