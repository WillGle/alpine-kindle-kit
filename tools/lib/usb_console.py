#!/usr/bin/env python3
"""Physical Alpine USB console; fixed device identity and nonce-framed commands."""
from pathlib import Path
import os
import errno
import re
import select
import secrets
import shlex
import time
import tty

class Console:
    def __init__(self, seconds=30):
        self.fd = None
        deadline = time.monotonic() + seconds
        challenge = secrets.token_hex(16)
        while time.monotonic() < deadline:
            for entry in Path('/sys/class/tty').glob('ttyACM*'):
                usb = next((p for p in entry.resolve().parents if (p/'idVendor').exists()), None)
                if usb is None or (usb/'idVendor').read_text().strip() != '0525' or (usb/'idProduct').read_text().strip() != 'a4a7':
                    continue
                try:
                    fd = os.open('/dev/'+entry.name, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
                except OSError as error:
                    # USB sysfs registration can precede devtmpfs/udev creation.
                    if error.errno in (errno.ENOENT, errno.ENODEV, errno.EACCES):
                        continue
                    raise
                tty.setraw(fd)
                # A cable drop can leave the previous physical shell alive.
                # Exit it before requesting a fresh protocol challenge.
                os.write(fd, b'\nexit\n')
                time.sleep(.2)
                os.write(fd, (challenge+'\n').encode())
                output = b''
                until = time.monotonic()+4
                while time.monotonic() < until:
                    ready, _, _ = select.select([fd], [], [], .2)
                    if ready:
                        data = os.read(fd, 65536)
                        if not data:
                            break
                        output += data
                    if ('pid1=/bin/busybox challenge='+challenge).encode() in output:
                        self.fd = fd
                        os.write(fd, b'SHELL\n')
                        time.sleep(.2)
                        return
                os.close(fd)
            time.sleep(.3)
        raise TimeoutError('No verified Alpine physical console')

    def run(self, script, seconds=30):
        nonce = secrets.token_hex(12)
        command = "( /bin/busybox sh -c " + shlex.quote(script) + "); rc=$?; printf '\\nGW_DONE_%s:%s\\n' " + nonce + ' "$rc"\n'
        data = command.encode()
        while data:
            _, ready, _ = select.select([], [self.fd], [], 2)
            if not ready:
                raise TimeoutError('USB command write stalled')
            count = os.write(self.fd, data[:2048])
            data = data[count:]
        output = b''
        until = time.monotonic()+seconds
        pattern = re.compile(rb'\r?\nGW_DONE_'+nonce.encode()+rb':([0-9]+)\r?\n')
        while time.monotonic() < until:
            ready, _, _ = select.select([self.fd], [], [], .2)
            if ready:
                data = os.read(self.fd, 65536)
                if not data:
                    raise EOFError('USB disconnected')
                output += data
                match = pattern.search(output)
                if match:
                    text = output[:match.start()].decode(errors='replace')
                    if int(match[1]):
                        raise RuntimeError(text+'\nDevice exit '+match[1].decode())
                    return text
        raise TimeoutError('USB command timed out: '+output[-2000:].decode(errors='replace'))

    def put(self, data, destination, mode=0o644):
        import base64
        import hashlib
        temporary = destination+'.native-upload'
        expected = hashlib.sha256(data).hexdigest()
        # The interactive shell drops oversized input; keep each command bounded.
        self.run("set -C; : > " + shlex.quote(temporary))
        for start in range(0, len(data), 384):
            encoded = base64.b64encode(data[start:start+384]).decode()
            self.run("printf '%s' " + shlex.quote(encoded) +
                     " | /bin/busybox base64 -d >> " + shlex.quote(temporary))
        script = "set -eu\n[ \"$(/bin/busybox sha256sum " + shlex.quote(temporary) + " | /bin/busybox awk '{print $1}')\" = " + expected + " ]\n"
        script += "/bin/busybox chmod " + oct(mode)[2:] + " " + shlex.quote(temporary) + "\n/bin/busybox mv " + shlex.quote(temporary) + " " + shlex.quote(destination)
        return self.run(script)

    def close(self):
        if self.fd is not None:
            try:
                os.write(self.fd, b'exit\n')
                time.sleep(.1)
                ready, _, _ = select.select([self.fd], [], [], .2)
                if ready:
                    os.read(self.fd, 65536)
            finally:
                os.close(self.fd)
                self.fd = None

if __name__ == '__main__':
    import sys
    console = Console()
    try:
        print(console.run(sys.stdin.read()))
    finally:
        console.close()
