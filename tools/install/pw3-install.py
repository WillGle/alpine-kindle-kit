#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Guarded first installation on rooted PW3 (0KB), firmware 5.16.2.1.1. Linux host only."""
from pathlib import Path
import argparse
import contextlib
import fcntl
import hashlib
import io
import json
import os
import re
import secrets
import shlex
import shutil
import subprocess
import sys
sys.dont_write_bytecode = True
import tarfile
import time
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'lib'))
from pw3_common import DEVICE_PROFILE, KERNEL, ROOT, TOOLS, STOCK_INIT, TOKEN, add_work_dir, workspace, clean_image, digest, require, run, save, syncdir

from usb_console import Console

PROFILE = DEVICE_PROFILE['name']
PAYLOAD = {'bootstrap','shutdown','guard-control','gleworks_pid1_reset.ko','fsl_otg_arc.ko',
           'arcotg_udc.ko','g_file_storage.ko','dispatcher.sh','install-default-trial.sh',
           'restore-stock-init.sh','return-control.conf','system.ext3','SHA256SUMS'}
BAD_IMAGE = b'PW3 INSTALLER DELIBERATELY INVALID TRIAL IMAGE\n'
BAD_SHA = hashlib.sha256(BAD_IMAGE).hexdigest()
q = shlex.quote

# Only hashes of device identifiers leave the device. The model code is public.
STOCK_PROBE = r'''set -eu
[ "$(id -u)" = 0 ]
[ "$(uname -r)" = 3.0.35-lab126 ]
[ "$(readlink /proc/1/exe)" = /sbin/init.exe ]
grep -q '^/dev/root / ext3 ro,' /proc/mounts
grep -q '5\.16\.2\.1\.1\([^0-9.]\|$\)' /etc/prettyversion.txt
grep -qi wario /proc/cpuinfo
[ "$(cut -c 1-6 /proc/usid)" = G090KB ]
[ "$(cat /sys/class/block/mmcblk0p1/start)" = 65536 ]
[ "$(cat /sys/class/block/mmcblk0p1/size)" = 921600 ]
[ "$(cat /sys/class/block/mmcblk0p4/size)" = 6385664 ]
[ "$(cat /sys/class/power_supply/max77696-battery/capacity)" -ge 50 ]
grep -q ' /mnt/us .*rw,' /proc/mounts
printf 'identity='; sha256sum /proc/usid | awk '{print $1}'
printf 'boot='; cat /proc/sys/kernel/random/boot_id
printf 'init='; sha256sum /sbin/init | awk '{print $1}'
printf 'free_kib='; df -k /mnt/us | awk 'END {print $4}'
'''


class Installer:
    def __init__(self, directory):
        self.directory = directory.absolute()
        require(self.directory.is_dir() and not self.directory.is_symlink(), 'Session directory missing or symlinked')
        require(self.directory.stat().st_uid == os.getuid() and self.directory.stat().st_mode & 0o077 == 0,
                'Session directory must be owned by this user, mode 0700')
        self.path = self.directory/'session.json'
        self.state = json.loads(self.path.read_text())
        require(self.state.get('schema') == 1 and self.state.get('profile') == PROFILE, 'Unsupported session')
        require(re.fullmatch('[0-9a-f]{32}', self.state['id']), 'Invalid session identity')
        self.ssh = ['ssh','-F','/dev/null','-i',str(self.directory/'client-key'),
                    '-o','IdentitiesOnly=yes','-o','BatchMode=yes','-o','StrictHostKeyChecking=yes',
                    '-o','UserKnownHostsFile='+str(self.directory/'known_hosts'),
                    '-o','ConnectTimeout=8','-p',str(self.state['port']),'root@'+self.state['host']]

    def persist(self):
        save(self.path, self.state)

    def phase(self, *phases):
        require(self.state['phase'] in phases, 'Step requires '+', '.join(phases)+'; current phase: '+self.state['phase'])
        require(not self.state.get('pending'), 'Interrupted operation: '+str(self.state.get('pending'))+'; use status/reconcile, do not blindly retry')

    def begin(self, operation):
        self.state['pending'] = operation
        self.persist()

    def finish(self, phase, evidence=None):
        if evidence is not None:
            self.state.setdefault('evidence', {})[phase] = evidence
        self.state['phase'] = phase
        self.state.pop('pending', None)
        self.persist()
        print('Phase:', phase)

    def remote(self, script, timeout=45):
        result = run(self.ssh+['sh -s'], input=script, text=True, timeout=timeout)
        return result.stdout

    def stock(self, init=None, changed_boot=None):
        text = self.remote(STOCK_PROBE)
        values = dict(line.split('=',1) for line in text.splitlines())
        require(re.fullmatch('[0-9a-f]{64}', values['identity']), 'Invalid device identity')
        require(re.fullmatch('[0-9a-f-]{36}', values['boot']), 'Invalid boot identity')
        if 'identity' in self.state:
            require(values['identity'] == self.state['identity'], 'Different PW3 connected')
        if init:
            require(values['init'] == init, 'Unexpected init; do not overwrite unknown state')
        if changed_boot:
            require(values['boot'] != changed_boot, 'A fresh cold boot has not been observed')
        return values

    def guard(self, init):
        # Repeat identity/layout checks inside each remote write command.
        return STOCK_PROBE+f'''[ "$(sha256sum /proc/usid | awk '{{print $1}}')" = {q(self.state['identity'])} ]
[ "$(sha256sum /sbin/init | awk '{{print $1}}')" = {q(init)} ]
'''

    def candidate(self):
        path = Path(self.state['candidate'])
        require(digest(path/'candidate.json') == self.state['candidate_sha256'], 'Candidate manifest changed')
        manifest = json.loads((path/'candidate.json').read_text())
        require(manifest.get('variant', 'reader') in {'base','reader'}, 'Unsupported OS variant')
        require(manifest['profile'] == PROFILE and set(manifest['files']) == PAYLOAD, 'Unexpected candidate payload')
        for name, wanted in manifest['files'].items():
            require(digest(path/name) == wanted, 'Candidate changed: '+name)
        require(manifest['inputs']['p1_backup'] == self.state['backups']['p1.ext3'], 'Candidate built from another p1 backup')
        require(manifest['inputs']['modules'] == self.state['backups']['stock-modules.tar'], 'Candidate built from another module capture')
        return path, manifest

    def backup(self):
        self.phase('inspected')
        before = self.stock(STOCK_INIT)
        require(shutil.disk_usage(self.directory).free > 5*1024**3, 'Need at least 5 GiB free host storage for backups')
        dest = self.directory/'backup'
        require(not dest.exists(), 'Existing backup retained; do not overwrite it')
        self.begin('backup')
        dest.mkdir(mode=0o700)
        sources = [('p1.ext3','dd if=/dev/mmcblk0p1 bs=1048576',471859200),
                   ('stock-modules.tar','tar -C / -cf - lib/modules/3.0.35-lab126',None),
                   ('userstore.tar','tar -C /mnt/us -cf - .',None),
                   ('amazon-state-live.tar','tar -C /var/local -cf - .',None)]
        hashes = {}
        for name, command, size in sources:
            partial = dest/(name+'.partial')
            # Recheck before every stream. Probe stdout must not prefix binary data.
            with partial.open('xb') as stream:
                result = subprocess.run(self.ssh+['sh -c '+q('{\n'+self.guard(STOCK_INIT)+'\n} >/dev/null\n'+command)],
                                        stdout=stream, stderr=subprocess.PIPE, timeout=1800)
                stream.flush()
                os.fsync(stream.fileno())
            require(result.returncode == 0, 'Backup failed; partial retained, no device writes: '+name)
            require(partial.stat().st_size > 0 and (size is None or partial.stat().st_size == size), 'Incomplete backup: '+name)
            require(self.stock(STOCK_INIT)['boot'] == before['boot'], 'Device rebooted during backup')
            partial.rename(dest/name)
            hashes[name] = digest(dest/name)
        self.state['backups'] = hashes
        self.finish('backed-up', {'boot':before['boot'],'files':hashes,
                    'amazon_state_consistency':'Live file archive; not a certified Amazon full restore'})

    def bind(self, path):
        self.phase('backed-up')
        self.stock(STOCK_INIT)
        for name, wanted in self.state['backups'].items():
            require(digest(self.directory/'backup'/name) == wanted, 'Backup hash mismatch: '+name)
        self.state['candidate'] = str(path.absolute())
        self.state['candidate_sha256'] = digest(path/'candidate.json')
        _, manifest = self.candidate()
        clean_image(path/'system.ext3')
        launcher = '/mnt/us/documents/gleworks-alpine-session.sh'
        self.remote(self.guard(STOCK_INIT)+f'[ "$(sha256sum {launcher} | awk \'{{print $1}}\')" = {q(manifest["return_launcher_sha256"])} ]\n')
        self.finish('ready')

    def install(self):
        self.phase('ready')
        _, m = self.candidate()
        path = Path(self.state['candidate'])
        before = self.stock(STOCK_INIT)
        require(int(before['free_kib']) >= 600*1024, 'Need at least 600 MiB free userstore')
        for name, wanted in self.state['backups'].items():
            require(digest(self.directory/'backup'/name) == wanted, 'Backup changed: '+name)
        # Validate all conflicts before the first device write.
        stage = '/tmp/pw3-install-'+self.state['id']
        self.remote(self.guard(STOCK_INIT)+f'''for p in {q(stage)} /mnt/us/gleworks-alpine /gleworks-native-trial /sbin/init.gleworks-stock; do
[ ! -e "$p" ] && [ ! -L "$p" ]
done
''')
        self.begin('install-dispatcher')
        buf = io.BytesIO()
        with tarfile.open(fileobj=buf, mode='w') as tar:
            for name in sorted(PAYLOAD-{'system.ext3'}):
                tar.add(path/name, arcname=name, recursive=False)
        script = self.guard(STOCK_INIT)+f'\numask 077\nmkdir {q(stage)}\ntar -C {q(stage)} -xf -\n'
        run(self.ssh+['sh -c '+q(script)], input=buf.getvalue(), timeout=120)
        target = '/mnt/us/gleworks-alpine'
        script = self.guard(STOCK_INIT)+f'''umask 077
mkdir {target}
printf '%s\\n' {self.state['id']} > {target}/installer-session
cat > {target}/system.partial
sync
[ "$(sha256sum {target}/system.partial | awk '{{print $1}}')" = {m['files']['system.ext3']} ]
mv {target}/system.partial {target}/system.ext3
sync
'''
        with (path/'system.ext3').open('rb') as stream:
            subprocess.run(self.ssh+['sh -c '+q(script)], stdin=stream, stdout=subprocess.DEVNULL,
                           stderr=subprocess.PIPE, check=True, timeout=600)
        self.remote(self.guard(STOCK_INIT)+f'''cd {q(stage)}
[ "$(sha256sum SHA256SUMS | awk '{{print $1}}')" = {m['files']['SHA256SUMS']} ]
sha256sum -c SHA256SUMS
sh ./install-default-trial.sh {q(stage)}
[ "$(sha256sum /sbin/init | awk '{{print $1}}')" = {m['files']['dispatcher.sh']} ]
''', timeout=180)
        self.state['last_stock_boot'] = before['boot']
        self.finish('dispatcher-installed', {'boot':before['boot'],'candidate':self.state['candidate_sha256']})

    def reboot_stock(self):
        self.phase('dispatcher-installed')
        _, m = self.candidate()
        before = self.stock(m['files']['dispatcher.sh'])
        self.begin('stock-reboot')
        self.remote(self.guard(m['files']['dispatcher.sh'])+'''[ ! -e /gleworks-native-trial/try.once ]
[ ! -e /mnt/us/gleworks-alpine/boot.once ]
umask 077
printf '%s\n' gleworks-coldboot-control-v1 > /var/local/gleworks-coldboot-control.once
sync
''')
        self.state['last_stock_boot'] = before['boot']
        self.finish('awaiting-stock')
        self.request_reboot(m['files']['dispatcher.sh'])

    def request_reboot(self, init):
        # Record expectation before asking the device to leave this connection.
        try:
            self.remote(self.guard(init)+'/sbin/reboot\n', timeout=15)
        except (subprocess.SubprocessError, OSError):
            print('Reboot connection ended; verify the next boot before continuing.')

    def verify_stock(self, rescue=False):
        self.phase('awaiting-rescue-return' if rescue else 'awaiting-stock', *([] if rescue else ['trial-observed']))
        _, m = self.candidate()
        before = self.stock(m['files']['dispatcher.sh'], changed_boot=self.state['last_stock_boot'])
        script = self.guard(m['files']['dispatcher.sh'])+f'''[ "$(sha256sum /sbin/init.gleworks-stock | awk '{{print $1}}')" = {STOCK_INIT} ]
[ ! -e /gleworks-native-trial/try.once ]
[ ! -e /mnt/us/gleworks-alpine/boot.once ]
[ ! -e /mnt/us/gleworks-alpine/boot.pending ]
[ "$(cat /mnt/us/gleworks-alpine/installer-session)" = {self.state['id']} ]
[ "$(sha256sum /mnt/us/gleworks-alpine/system.ext3 | awk '{{print $1}}')" = {m['files']['system.ext3']} ]
'''
        if rescue:
            proof = self.state['repair']
            script += f'[ "$(cat /mnt/us/gleworks-alpine/{proof["filename"]})" = {q(proof["payload"].strip())} ]\n'
        self.remote(script, timeout=180)
        phase = 'rescue-qualified' if rescue else ('trial-qualified' if self.state['phase'] == 'trial-observed' else 'stock-qualified')
        self.state['last_stock_boot'] = before['boot']
        self.finish(phase, {'boot':before['boot'],'identity':self.state['identity'],
                           'candidate':self.state['candidate_sha256']})

    def arm(self, rescue=False):
        self.phase('trial-qualified' if rescue else 'stock-qualified')
        _, m = self.candidate()
        before = self.stock(m['files']['dispatcher.sh'])
        self.state['last_stock_boot'] = before['boot']
        self.begin('rescue-trial' if rescue else 'native-trial')
        script = self.guard(m['files']['dispatcher.sh'])+f'''[ "$(sha256sum /gleworks-native-trial/bootstrap | awk '{{print $1}}')" = {m['files']['bootstrap']} ]
[ "$(sha256sum /mnt/us/gleworks-alpine/system.ext3 | awk '{{print $1}}')" = {m['files']['system.ext3']} ]
[ "$(sha256sum /mnt/us/documents/gleworks-alpine-session.sh | awk '{{print $1}}')" = {m['return_launcher_sha256']} ]
for p in /gleworks-native-trial/try.once /mnt/us/gleworks-alpine/boot.once /mnt/us/gleworks-alpine/boot.enabled /mnt/us/gleworks-alpine/boot.pending /var/local/gleworks-coldboot-control.once; do
[ ! -e "$p" ] && [ ! -L "$p" ]
done
umask 077
'''
        if rescue:
            good = '/mnt/us/gleworks-alpine/system.valid-'+self.state['id']+'.ext3'
            script += f'''[ ! -e {good} ] && [ ! -L {good} ]
mv /mnt/us/gleworks-alpine/system.ext3 {good}
printf '%s' {q(BAD_IMAGE.decode())} > /mnt/us/gleworks-alpine/system.ext3
sync
[ "$(sha256sum /mnt/us/gleworks-alpine/system.ext3 | awk '{{print $1}}')" = {BAD_SHA} ]
'''
        script += '''printf '%s\n' gleworks-alpine-v1 > /mnt/us/gleworks-alpine/boot.once
printf '%s\n' gleworks-coldboot-control-v1 > /var/local/gleworks-coldboot-control.once
sync
trap 'sync; /bin/mount -o remount,ro /' EXIT
/bin/mount -o remount,rw /
'''
        request = 'gleworks-usb-rescue-repair-v1' if rescue else 'one supervised coldboot only'
        script += f"printf '%s\\n' {q(request)} > /gleworks-native-trial/try.once\nsync\n/bin/mount -o remount,ro /\n"
        self.remote(script, timeout=180)
        self.finish('rescue-armed' if rescue else 'trial-armed')
        self.request_reboot(m['files']['dispatcher.sh'])

    def console(self, persistent=False):
        with contextlib.closing(Console(seconds=30)) as console:
            script = f'''set -eu
[ "$(sha256sum /proc/usid | awk '{{print $1}}')" = {self.state['identity']} ]
[ "$(uname -r)" = {KERNEL} ]
[ "$(readlink /proc/1/exe)" = /bin/busybox ]
[ "$(cat /boot/gleworks-control/gleworks-alpine/installer-session)" = {self.state['id']} ]
cat /proc/sys/kernel/random/boot_id
cat /proc/mounts
/gleworks/guard-control status
'''
            if persistent:
                _, candidate = self.candidate()
                script += '''grep -q '^/dev/loop[^ ]* / ext3 rw,' /proc/mounts
[ -f /run/persistent-boot ]
awk '{if ($1 < 100) exit 1}' /proc/uptime
'''
                if candidate.get('variant', 'reader') == 'reader':
                    script += '[ -s /run/reader.log ]\ngrep -q GW_NATIVE_DISPLAY_START /run/reader.log\n'
            else:
                script += "grep -q '^/dev/loop[^ ]* / ext3 ro,' /proc/mounts\n"
            text = console.run(script)
            require(('state=00000000' if persistent else 'state=00000002') in text, 'Unexpected guardian state')
            return text

    def observe(self):
        self.phase('trial-armed')
        self.candidate()
        text = self.console()
        self.finish('trial-observed', {'fresh_usb_nonce':True,'output':text,
                                     'identity':self.state['identity'],'candidate':self.state['candidate_sha256']})
        print('Keep USB attached. Guardian returns to stock; then run verify-stock.')

    def repair(self):
        self.phase('rescue-armed')
        path, m = self.candidate()
        maintenance = load_maintenance()
        rescue = maintenance.Rescue(seconds=30)
        control = rescue.control
        require((control/'installer-session').read_text().strip() == self.state['id'], 'Wrong rescue session')
        require(digest(control/'system.ext3') == BAD_SHA, 'Expected deliberately invalid trial image')
        require(digest(control/('system.valid-'+self.state['id']+'.ext3')) == m['files']['system.ext3'], 'Saved valid root missing or changed')
        nonce = secrets.token_hex(16)
        filename = 'installer-rescue-'+nonce+'.txt'
        payload = self.state['id']+' '+nonce+' '+m['files']['system.ext3']+'\n'
        self.state['repair'] = {'filename':filename,'payload':payload}
        self.begin('rescue-repair')
        partial = control/'system.install-repair.partial'
        with partial.open('xb') as dst, (path/'system.ext3').open('rb') as src:
            while data := src.read(1024*1024):
                rescue.same()
                dst.write(data)
            dst.flush()
            os.fsync(dst.fileno())
        rescue.same()
        require(digest(partial) == m['files']['system.ext3'], 'Rescue repair hash mismatch')
        os.replace(partial, control/'system.ext3')
        with (control/filename).open('x') as stream:
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
        require((control/filename).read_text() == payload, 'Rescue challenge readback failed')
        syncdir(control)
        rescue.same()
        rescue.eject()
        self.finish('awaiting-rescue-return', {'usb':'0525:a4a5 bcd0200','nonce':nonce,
                                             'candidate':self.state['candidate_sha256'],'safe_eject':True})

    def promote(self):
        self.phase('rescue-qualified')
        _, m = self.candidate()
        self.stock(m['files']['dispatcher.sh'])
        for name, wanted in self.state['backups'].items():
            require(digest(self.directory/'backup'/name) == wanted, 'Backup changed: '+name)
        require(self.state['evidence']['rescue-qualified']['candidate'] == self.state['candidate_sha256'], 'Recovery proof is for another build')
        self.begin('promote')
        script = self.guard(m['files']['dispatcher.sh'])+'\n'+(TOOLS/'install/install-native-entry.sh').read_text()
        # Positional parameters are supplied to the script, never interpolated as shell code.
        args = ['/gleworks-native-trial/bootstrap',m['files']['bootstrap'],m['files']['system.ext3'],m['files']['dispatcher.sh']]
        self.remote('set -- '+' '.join(q(a) for a in args)+'\n'+script, timeout=180)
        self.finish('promoted', {'candidate':self.state['candidate_sha256']})

    def activate(self):
        self.phase('promoted')
        _, m = self.candidate()
        self.stock(m['files']['bootstrap'])
        self.begin('activate')
        self.finish('awaiting-native')
        self.request_reboot(m['files']['bootstrap'])

    def verify_native(self):
        self.phase('awaiting-native','awaiting-final-native')
        self.candidate()
        text = self.console(persistent=True)
        self.finish('installed' if self.state['phase'] == 'awaiting-final-native' else 'native-observed',
                    {'fresh_usb_nonce':True,'output':text,'reader_physical_acceptance':False,
                     'candidate':self.state['candidate_sha256']})
        print('Screen/touch/book acceptance still requires the user; host proof cannot certify the panel.')

    def native_rescue(self):
        self.phase('native-observed')
        self.candidate()
        console = Console(seconds=30)
        try:
            # Verify and mutate through the same open USB connection. Reopening
            # an unbound console here could select another attached PW3.
            identity = f'''set -eu
[ "$(sha256sum /proc/usid | awk '{{print $1}}')" = {self.state['identity']} ]
[ "$(readlink /proc/1/exe)" = /bin/busybox ]
[ "$(uname -r)" = {KERNEL} ]
[ "$(cat /boot/gleworks-control/gleworks-alpine/installer-session)" = {self.state['id']} ]
[ "$(cat /boot/gleworks-control/gleworks-alpine/boot.enabled)" = gleworks-alpine-v1 ]
'''
            console.run(identity)
            self.begin('native-rescue')
            console.run(identity+'''rm /boot/gleworks-control/gleworks-alpine/boot.enabled
sync
echo GW_INSTALLER_RESCUE_SELECTED
''')
            os.write(console.fd, b'/bin/busybox kill -QUIT 1\n')
        finally:
            if console.fd is not None:
                os.close(console.fd)
                console.fd = None
        module = load_maintenance()
        rescue = module.Rescue(seconds=30)
        require((rescue.control/'installer-session').read_text().strip() == self.state['id'], 'Wrong rescue session; no gate recorded')
        require(not (rescue.control/'boot.pending').exists(), 'Final rescue is not from a clean shutdown')
        self.finish('final-rescue')

    def native_resume(self):
        self.phase('final-rescue')
        self.candidate()
        module = load_maintenance()
        rescue = module.Rescue(seconds=30)
        require((rescue.control/'installer-session').read_text().strip() == self.state['id'], 'Wrong rescue session')
        require(not (rescue.control/'boot.pending').exists(), 'Unclean shutdown; offline repair required')
        self.begin('native-resume')
        module.resume_rescue(rescue)
        self.finish('awaiting-final-native')

    def reconcile(self):
        operation = self.state.get('pending')
        require(operation, 'No interrupted operation to reconcile')
        _, m = self.candidate()
        # Reconciliation only accepts fully completed postconditions; never clears
        # a partial install or writes to the device to guess what happened.
        if operation == 'install-dispatcher':
            self.stock(m['files']['dispatcher.sh'])
            self.remote(self.guard(m['files']['dispatcher.sh'])+f'''[ "$(sha256sum /sbin/init.gleworks-stock | awk '{{print $1}}')" = {STOCK_INIT} ]
[ "$(sha256sum /gleworks-native-trial/bootstrap | awk '{{print $1}}')" = {m['files']['bootstrap']} ]
[ "$(sha256sum /mnt/us/gleworks-alpine/system.ext3 | awk '{{print $1}}')" = {m['files']['system.ext3']} ]
[ "$(cat /mnt/us/gleworks-alpine/installer-session)" = {self.state['id']} ]
[ ! -e /gleworks-native-trial/try.once ]
''', timeout=180)
            self.finish('dispatcher-installed')
        elif operation == 'promote':
            self.stock(m['files']['bootstrap'])
            self.remote(self.guard(m['files']['bootstrap'])+f'''[ "$(cat /mnt/us/gleworks-alpine/boot.enabled)" = gleworks-alpine-v1 ]
[ "$(sha256sum /sbin/init.gleworks-dispatch | awk '{{print $1}}')" = {m['files']['dispatcher.sh']} ]
''')
            self.finish('promoted')
        else:
            raise ValueError('Cannot safely reconcile '+operation+' automatically. Preserve session/backups and inspect the documented recovery state; no writes performed.')


def load_maintenance():
    import importlib.util
    spec = importlib.util.spec_from_file_location('kindle_maintenance', TOOLS/'maintenance/kindle-linux.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def initialize(args):
    require(sys.platform.startswith('linux'), 'Linux host required')
    require(re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9.-]*', args.host), 'Invalid SSH hostname/address')
    require(1 <= args.port <= 65535, 'Invalid SSH port')
    require(not args.session.exists(), 'Session must be a new directory')
    for path in [args.key, args.known_hosts]:
        digest(path)
    args.session.mkdir(parents=True, mode=0o700)
    for source, name in [(args.key,'client-key'),(args.known_hosts,'known_hosts')]:
        shutil.copyfile(source, args.session/name)
        (args.session/name).chmod(0o600)
    save(args.session/'session.json', {'schema':1,'id':secrets.token_hex(16),'profile':PROFILE,
                                     'host':args.host,'port':args.port,'phase':'new'})
    installer = Installer(args.session)
    values = installer.stock(STOCK_INIT)
    installer.state['identity'] = values['identity']
    installer.state['initial_boot'] = values['boot']
    installer.finish('inspected', values)


def main():
    os.umask(0o077)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--session', type=Path, help='Private per-device directory; defaults to work-dir/private/sessions/pw3')
    add_work_dir(parser)
    subs = parser.add_subparsers(dest='command', required=True)
    p = subs.add_parser('init', help='Read-only stock preflight and create private host session')
    p.add_argument('--host', required=True)
    p.add_argument('--port', type=int, default=2222)
    p.add_argument('--key', type=Path, required=True)
    p.add_argument('--known-hosts', type=Path, required=True)
    subs.add_parser('status', help='Show phase without connecting to device')
    subs.add_parser('backup', help='Read p1, modules, books and live Amazon state to host')
    p = subs.add_parser('bind', help='Bind a fresh candidate built from this session backups')
    p.add_argument('candidate', type=Path)
    for name, help_text in [
        ('install-dispatcher','WRITE: stage root and install stock-default dispatcher'),
        ('stock-reboot','WRITE/REBOOT: qualify default return to stock'),
        ('verify-stock','Read fresh stock return after default boot or readonly trial'),
        ('trial','WRITE/REBOOT: one readonly Alpine trial with guardian'),
        ('observe-trial','Read fresh USB proof before the guardian returns to stock'),
        ('rescue-trial','WRITE/REBOOT: preserve good image and deliberately try invalid image'),
        ('repair','WRITE/EJECT: repair invalid image through independent USB rescue'),
        ('verify-rescue-return','Read stock return and the rescue nonce'),
        ('promote','WRITE: switch default init after all recovery gates'),
        ('activate','REBOOT: start persistent Alpine'),
        ('verify-native','Read native proof after at least 100 seconds uptime'),
        ('native-rescue','WRITE/REBOOT: verify clean installed-system rescue'),
        ('native-resume','WRITE/EJECT: resume installed Alpine from final rescue'),
        ('reconcile','Read-only recognition of a completed interrupted install/promote')]:
        subs.add_parser(name, help=help_text)
    args = parser.parse_args()
    args.session = args.session or workspace(args.work_dir)/'private/sessions/pw3'
    try:
        # This lock covers this coordinator; do not run legacy maintenance concurrently.
        with open('/tmp/gleworks-pw3-installer.lock','a') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            if args.command == 'init':
                initialize(args)
                return
            installer = Installer(args.session)
            if args.command == 'status':
                print(json.dumps({'profile':PROFILE,'phase':installer.state['phase'],
                                  'pending':installer.state.get('pending'),
                                  'physical_reader_acceptance':'Not certified by this CLI'}, indent=2))
            elif args.command == 'bind':
                installer.bind(args.candidate)
            elif args.command == 'verify-rescue-return':
                installer.verify_stock(rescue=True)
            else:
                methods = {'install-dispatcher':'install','stock-reboot':'reboot_stock','trial':'arm',
                           'observe-trial':'observe','rescue-trial':'arm','verify-stock':'verify_stock',
                           'verify-native':'verify_native','native-rescue':'native_rescue','native-resume':'native_resume'}
                if args.command == 'rescue-trial':
                    installer.arm(rescue=True)
                else:
                    getattr(installer, methods.get(args.command,args.command.replace('-','_')))()
    except (ValueError, KeyError, RuntimeError, OSError, subprocess.SubprocessError, tarfile.TarError) as error:
        parser.exit(1, 'STOP: '+str(error)+'\nNo automatic retry. Preserve session/backups; run status and follow recovery instructions.\n')


if __name__ == '__main__':
    main()
