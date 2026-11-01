#!/usr/bin/env python3
"""PW3 native Alpine maintenance. Mutations require the independent rescue USB."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import select
import shutil
import stat
import subprocess
import sys
sys.dont_write_bytecode = True
import tarfile
import time

BASE = Path(__file__).resolve().parent
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'lib'))
from pw3_common import ROOT, TOOLS, add_work_dir, workspace
from usb_console import Console
TOKEN = b'gleworks-alpine-v1\n'


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        while data := stream.read(1024*1024):
            h.update(data)
    return h.hexdigest()


def call(args, **kw):
    return subprocess.run(args, check=True, capture_output=True, text=True, **kw)


def clean_image(path):
    assert path.is_file() and not path.is_symlink(), 'Root image must be a regular file'
    assert 64*1024*1024 <= path.stat().st_size <= 1024*1024*1024, 'Unsupported image size'
    with path.open('rb') as f:
        f.seek(1024)
        sb = f.read(104)
    assert sb[56:60] == b'\x53\xef\x01\x00' and not sb[96]&4, 'Root requires offline e2fsck'
    call(['e2fsck', '-fn', str(path)])


def syncdir(path):
    fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


class Rescue:
    def __init__(self, seconds=90):
        until = time.monotonic()+seconds
        while time.monotonic() < until:
            for block in Path('/sys/class/block').glob('sd?'):
                usb = next((p for p in block.resolve().parents if (p/'idVendor').exists()), None)
                if usb is None or (usb/'idVendor').read_text().strip() != '0525' or (usb/'idProduct').read_text().strip() != 'a4a5':
                    continue
                if (usb/'bcdDevice').read_text().strip() != '0200':
                    continue
                assert int((block/'size').read_text())*512 == 3269459968, 'Wrong PW3 capacity'
                self.block, self.instance, self.usb = block, block.resolve(), usb
                self.devnum = (usb/'devnum').read_text().strip()
                self.node = '/dev/'+block.name
                self.partition = self.node+'1'
                self.mount = None
                for _ in range(60):
                    self.same()
                    # The block node can precede its UDisks registration.
                    registered = subprocess.run(['udisksctl', 'info', '-b', self.node],capture_output=True,text=True)
                    if registered.returncode != 0:
                        time.sleep(.3)
                        continue
                    info = registered.stdout
                    drive = re.search(r"Drive:\s+'([^']+)'", info)
                    result = subprocess.run(['findmnt','-J','-S',self.partition,'-o','SOURCE,TARGET,FSTYPE'], capture_output=True, text=True)
                    if drive and re.search(r'HintSystem:\s+false', info) and result.returncode == 0:
                        fs = json.loads(result.stdout)['filesystems']
                        assert len(fs) == 1 and fs[0]['fstype'] == 'vfat' and fs[0]['source'] == self.partition
                        self.mount, self.drive = Path(fs[0]['target']), drive[1]
                        break
                    subprocess.run(['udisksctl','mount','-b',self.partition],capture_output=True)
                    time.sleep(.3)
                assert self.mount is not None, 'UDisks rescue mount not ready'
                self.control = self.mount/'gleworks-alpine'
                assert self.control.is_dir() and not self.control.is_symlink()
                return
            time.sleep(.3)
        raise TimeoutError('Independent USB rescue 0525:a4a5 bcd0200 not found')

    def same(self):
        assert self.block.resolve() == self.instance
        assert (self.usb/'devnum').read_text().strip() == self.devnum
        assert (self.usb/'idVendor').read_text().strip() == '0525'
        assert (self.usb/'idProduct').read_text().strip() == 'a4a5'
        assert (self.usb/'bcdDevice').read_text().strip() == '0200'

    def eject(self):
        self.same()
        call(['udisksctl','unmount','-b',self.partition])
        assert subprocess.run(['findmnt','-rn','-S',self.partition],capture_output=True).returncode == 1
        self.same()
        command = shutil.which('gdbus')
        if not command:
            candidates = list(Path('/nix/store').glob('*glib-*-bin/bin/gdbus'))
            assert candidates, 'gdbus is needed for safe SCSI eject'
            command = str(candidates[0])
        call([command,'call','--system','--dest','org.freedesktop.UDisks2','--object-path',self.drive,'--method','org.freedesktop.UDisks2.Drive.Eject','{}'])


def inspect(output):
    console = Console()
    try:
        text = console.run('''set -eu
cat /etc/alpine-release
readlink /proc/1/exe
cat /proc/sys/kernel/random/boot_id
cat /proc/uptime
cat /proc/mounts
cat /proc/modules
cat /proc/fb
cat /proc/bus/input/devices
ps
''')
    finally:
        console.close()
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(text)
    print(text)


def enter_rescue():
    for usb in Path('/sys/bus/usb/devices').iterdir():
        if (usb/'idVendor').exists() and (usb/'idVendor').read_text().strip()=='0525' and (usb/'idProduct').read_text().strip()=='a4a5' and (usb/'bcdDevice').read_text().strip()=='0200':
            rescue=Rescue()
            print('Independent USB rescue already ready:',rescue.mount)
            return
    console = Console()
    try:
        print(console.run('''set -eu
[ "$(readlink /proc/1/exe)" = /bin/busybox ]
[ "$(cat /boot/gleworks-control/gleworks-alpine/boot.enabled)" = gleworks-alpine-v1 ]
rm /boot/gleworks-control/gleworks-alpine/boot.enabled
sync
echo GW_RESCUE_SELECTED
'''))
        os.write(console.fd, b'/bin/busybox kill -QUIT 1\n')
        until = time.monotonic()+30
        while time.monotonic()<until:
            ready,_,_ = select.select([console.fd],[],[],.3)
            if ready:
                try:
                    if not os.read(console.fd,65536):
                        break
                except OSError:
                    break
    finally:
        if console.fd is not None:
            os.close(console.fd)
            console.fd = None
    rescue = Rescue()
    print('Independent USB rescue ready:', rescue.mount)


def deploy(args):
    image = args.image.resolve()
    clean_image(image)
    sha = digest(image)
    assert sha == args.sha256, 'Input hash mismatch'
    rescue = Rescue()
    rescue.same()
    assert not (rescue.control/'boot.enabled').exists(), 'Disable normal boot before replacement'
    assert not (rescue.control/'boot.once').exists(), 'Unexpected one-shot boot request'
    pending = rescue.control/'boot.pending'
    assert not (rescue.control/'boot.repairing').exists(), 'Interrupted repair: preserve evidence and verify storage offline before clearing boot.repairing'
    assert not pending.exists() or args.clear_pending, 'Pending crash; inspect/repair before explicit --clear-pending'
    assert re.fullmatch(r'[A-Za-z0-9._-]+\.ext3',args.backup_name) and args.backup_name != 'system.ext3'
    backup = rescue.control/args.backup_name
    assert not backup.exists(), 'Preserved image already exists; select a new backup name'
    free = shutil.disk_usage(rescue.control).free
    assert free >= image.stat().st_size + 64*1024*1024, (
        'Need %d MiB free userstore for the new image plus headroom; '
        'copy old system-*.ext3 backups to the host and remove them first'
        % ((image.stat().st_size + 64*1024*1024) // (1024*1024)))
    partial = rescue.control/'system.native-kit.partial'
    fd = os.open(partial,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600)
    with os.fdopen(fd,'wb') as target, image.open('rb') as source:
        while data := source.read(1024*1024):
            rescue.same()
            target.write(data)
        target.flush()
        os.fsync(target.fileno())
    assert digest(partial) == sha, 'Device readback mismatch; existing root was not replaced'
    rescue.same()
    current = rescue.control/'system.ext3'
    assert current.is_file() and not current.is_symlink()
    current.rename(backup)
    partial.rename(current)
    if pending.exists():
        pending.unlink()
    enabled = rescue.control/'boot.enabled'
    with enabled.open('xb') as f:
        f.write(TOKEN)
        f.flush()
        os.fsync(f.fileno())
    assert enabled.read_bytes() == TOKEN, 'Boot token readback mismatch; refusing to eject'
    syncdir(rescue.control)
    preserved = sorted(p.name for p in rescue.control.glob('*.ext3') if p.name != 'system.ext3')
    after_free = shutil.disk_usage(rescue.control).free // (1024*1024)
    proof = {'candidate_sha256':sha,'bytes':image.stat().st_size,'backup':args.backup_name,'usb':'0525:a4a5:0200','safe_eject':False}
    rescue.eject()
    proof['safe_eject'] = True
    args.evidence.parent.mkdir(parents=True,exist_ok=True)
    args.evidence.write_text(json.dumps(proof,indent=2)+'\n')
    print('Verified image installed; original preserved; safe eject requested boot.')
    print('Preserved images:', ', '.join(preserved) if preserved else 'none')
    print('Userstore free after install: %d MiB' % after_free)


def reboot_alpine():
    console=Console()
    result=console.run('cat /proc/sys/kernel/random/boot_id')
    before=re.search(r'\n([0-9a-f]{8}-(?:[0-9a-f]{4}-){3}[0-9a-f]{12})\r?\n',result).group(1)
    try:
        os.write(console.fd,b'/bin/busybox kill -QUIT 1\n')
        until=time.monotonic()+30
        while time.monotonic()<until:
            ready,_,_=select.select([console.fd],[],[],.3)
            if ready:
                try:
                    if not os.read(console.fd,65536):break
                except OSError:break
    finally:
        os.close(console.fd)
        console.fd=None
    after_console=Console(seconds=90)
    try:
        result=after_console.run('cat /proc/sys/kernel/random/boot_id; readlink /proc/1/exe; /gleworks/guard-control status')
    finally:after_console.close()
    after=re.search(r'\n([0-9a-f]{8}-(?:[0-9a-f]{4}-){3}[0-9a-f]{12})\r?\n',result).group(1)
    assert before!=after and '/bin/busybox' in result and 'state=00000000' in result,result
    print('Clean reboot verified:',before,'->',after)


def resume_rescue(rescue):
    rescue.same()
    assert not (rescue.control/'boot.repairing').exists(), 'Interrupted repair: preserve evidence and verify storage offline before clearing boot.repairing'
    assert not (rescue.control/'boot.pending').exists(), 'Crash pending: inspect/repair first'
    assert not (rescue.control/'boot.once').exists(), 'Unexpected one-shot request'
    clean_image(rescue.control/'system.ext3')
    enabled=rescue.control/'boot.enabled'
    if enabled.exists():
        assert not enabled.is_symlink() and enabled.read_bytes()==TOKEN
    else:
        with enabled.open('xb') as f:
            f.write(TOKEN)
            f.flush()
            os.fsync(f.fileno())
    assert enabled.read_bytes() == TOKEN, 'Boot token readback mismatch; refusing to eject'
    syncdir(rescue.control)
    rescue.eject()
    print('Clean Alpine selected; safe eject requested boot.')


def put_documents(paths):
    enter_rescue()
    rescue=Rescue()
    documents=rescue.mount/'documents'
    assert documents.is_dir() and not documents.is_symlink()
    for path in paths:
        source=path.resolve()
        assert source.is_file() and not path.is_symlink()
        target=documents/source.name
        assert not target.exists(), 'Document already exists: '+str(target)
        partial=documents/(source.name+'.native-upload')
        fd=os.open(partial,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600)
        with os.fdopen(fd,'wb') as output,source.open('rb') as input:
            while data:=input.read(1024*1024):
                rescue.same()
                output.write(data)
            output.flush()
            os.fsync(output.fileno())
        assert digest(partial)==digest(source)
        rescue.same()
        partial.rename(target)
        syncdir(documents)
        print('Document copied and hash verified:',source.name)
    resume_rescue(rescue)


def package(output, image=None):
    if image is not None:
        raise ValueError('Public source packaging excludes private images; retain the private candidate separately')
    import importlib.util
    spec = importlib.util.spec_from_file_location('public_source', TOOLS/'package/public-source.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.package(output)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    add_work_dir(parser)
    subs=parser.add_subparsers(dest='command',required=True)
    p=subs.add_parser('inspect');p.add_argument('--output',type=Path,default=None)
    subs.add_parser('rescue')
    subs.add_parser('reboot')
    subs.add_parser('resume')
    p=subs.add_parser('put-documents');p.add_argument('files',type=Path,nargs='+')
    p=subs.add_parser('deploy-root');p.add_argument('image',type=Path);p.add_argument('--sha256',required=True);p.add_argument('--backup-name',required=True);p.add_argument('--clear-pending',action='store_true');p.add_argument('--evidence',type=Path,default=None)
    p=subs.add_parser('package');p.add_argument('output',type=Path);p.add_argument('--image',type=Path)
    args=parser.parse_args()
    local = workspace(args.work_dir)/'private/maintenance'
    if args.command == 'inspect' and args.output is None: args.output = local/'last-inspection.log'
    if args.command == 'deploy-root' and args.evidence is None: args.evidence = local/'last-deployment.json'
    if args.command=='inspect':inspect(args.output)
    elif args.command=='rescue':enter_rescue()
    elif args.command=='reboot':reboot_alpine()
    elif args.command=='resume':resume_rescue(Rescue())
    elif args.command=='put-documents':put_documents(args.files)
    elif args.command=='deploy-root':deploy(args)
    elif args.command=='package':package(args.output,args.image)

if __name__=='__main__':
    main()
