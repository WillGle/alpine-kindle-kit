#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepare a private key-only stock SSH session offline; never connect to the Kindle."""
from pathlib import Path, PurePosixPath
import sys
sys.dont_write_bytecode = True
import argparse
import ipaddress
import os
import shutil
import subprocess
import tarfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'lib'))
from pw3_common import ALPINE_SHA, TOOLS, add_work_dir, workspace, digest, require, run, save


def prepare(a):
    os.umask(0o077)
    host = str(ipaddress.IPv4Address(a.host_address))
    device = str(ipaddress.IPv4Address(a.device_address))
    require(host != device and host.rsplit('.',1)[0] == device.rsplit('.',1)[0], 'Host and Kindle must be distinct addresses on the same /24 Wi-Fi network')
    require(digest(a.rootfs) == ALPINE_SHA, 'Pinned Alpine rootfs hash mismatch')
    for path in [a.dropbear, a.libz]:
        digest(path)
    require(not a.output.exists(), 'Retain existing control bundles; output must be new')
    a.output.mkdir(parents=True, mode=0o700)
    private = a.output/'private'
    bundle = a.output/'gleworks-alpine-session'
    private.mkdir()
    bundle.mkdir()
    run(['ssh-keygen','-q','-t','ed25519','-N','','-C','pw3-private-control','-f',private/'client-key'])
    run([a.dropbearkey,'-t','ed25519','-f',bundle/'server-key'])
    public = run([a.dropbearkey,'-y','-f',bundle/'server-key'], text=True).stdout
    public = next(line for line in public.splitlines() if line.startswith('ssh-ed25519 '))
    (private/'known_hosts').write_text(device+','+'['+device+']:2222 '+public+'\n')
    for source, name in [(a.dropbear,'dropbear'),(a.libz,'libz.so.1'),
                         (private/'client-key.pub','authorized_keys'),(TOOLS/'install/control-session/session.sh','session.sh')]:
        shutil.copyfile(source, bundle/name)
    with tarfile.open(a.rootfs) as tar:
        members = {str(PurePosixPath(m.name)):m for m in tar.getmembers()}
        for rel, name in [('bin/busybox','busybox'),('lib/ld-musl-armhf.so.1','ld-musl-armhf.so.1'),('etc/alpine-release','alpine-release')]:
            member = members[rel]
            require(member.isfile(), 'Expected regular Alpine binary')
            (bundle/name).write_bytes(tar.extractfile(member).read())
    files = sorted(bundle.iterdir())
    (bundle/'SHA256SUMS').write_text(''.join(digest(p)+'  '+p.name+'\n' for p in files))
    launcher = (TOOLS/'install/control-session/launcher.in.sh').read_text()
    launcher = launcher.replace('@HOST@', host).replace('@MANIFEST_HASH@', digest(bundle/'SHA256SUMS'))
    launcher = launcher.replace(' gleworks-alpine-stage.sh;', ';')
    launcher = launcher.replace('out=/mnt/us/gleworks-alpine-session-$(date +%Y%m%d-%H%M%S)-$$', 'out=$work/control-evidence')
    launcher = launcher.replace('nohup /bin/sh', 'GLEWORKS_CONTROL_ONLY=1 nohup /bin/sh')
    require('@' not in launcher and 'gleworks-alpine-stage.sh' not in launcher, 'Unrendered launcher')
    run(['sh','-n'], input=launcher, text=True)
    (a.output/'gleworks-alpine-session.sh').write_text(launcher)
    for path in [*private.iterdir(), *bundle.iterdir(),a.output/'gleworks-alpine-session.sh']:
        path.chmod(0o600)
    save(a.output/'control.json', {'schema':1,'host_address':host,'device_address':device,
                                 'port':2222,'lifetime_seconds':3600,'device_started':False,
                                 'launcher_sha256':digest(a.output/'gleworks-alpine-session.sh')})
    print('Private control bundle ready. Copy launcher + bundle into Kindle documents, then launch as root.')
    print('Never publish this directory. Session expires in one hour; only the specified host is allowed.')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ['host-address','device-address']:
        p.add_argument('--'+name, required=True)
    for name in ['dropbear','libz','rootfs']:
        p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--output', type=Path, help='New private control directory; defaults under work-dir/private/control')
    add_work_dir(p)
    p.add_argument('--dropbearkey', default='dropbearkey')
    args = p.parse_args()
    args.output = args.output or workspace(args.work_dir)/'private/control'
    try:
        prepare(args)
    except (ValueError, KeyError, OSError, subprocess.SubprocessError, tarfile.TarError, StopIteration) as error:
        p.exit(1, str(error)+'\n')


if __name__ == '__main__':
    main()
