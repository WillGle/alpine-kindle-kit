#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Package only reviewed PW3 installer source/docs; never include a device image or private input."""
from pathlib import Path
import sys
sys.dont_write_bytecode = True
import argparse
import io
import json
import tarfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'lib'))
from pw3_common import ROOT, digest, require

SOURCES = {
    'os/alpine': ['source.json','overlay/etc/inittab','overlay/etc/fstab'],
    'os/devices/pw3': ['profile.json','stock-kernel.config',
        'bootstrap/bootstrap.c','bootstrap/shutdown.c','bootstrap/install-default-trial.sh',
        'bootstrap/restore-stock-init.sh','bootstrap/trial-dispatch.in.sh','bootstrap/return-control.in.conf',
        'guardian/Makefile','guardian/gleworks_pid1_reset.c','guardian/reset-protocol.h',
        'guardian/guard-control.c','guardian/guard.h','guardian/sys.h',
        'overlay/etc/gleworks-early.sh','overlay/etc/gleworks-console.sh'],
    'apps/koreader': ['source.json','inittab.fragment','overlay/opt/koreader/native.lua',
        'overlay/opt/koreader/settings.reader.lua','overlay/etc/gleworks-display.sh',
        'overlay/etc/gleworks-reader-service.sh'],
    'tools/lib': ['pw3_common.py','usb_console.py'],
    'tools/build': ['pw3-build.py'],
    'tools/install': ['pw3-control.py','pw3-install.py','install-native-entry.sh',
                      'control-session/session.sh','control-session/launcher.in.sh'],
    'tools/maintenance': ['kindle-linux.py','capture-screen.py','clean-amazon-state.sh',
        'clean-legacy-storage.py','trim-reader-locales.py','stock-capture.py',
        'capture/gleworks-linux-probe.sh','capture/gleworks-boot-capture.sh',
        'capture/gleworks-module-capture.sh','capture/gleworks-waveform-capture.sh'],
    'tools/package': ['public-source.py','verify-package.py'],
}


def package(output):
    docs = ROOT/'docs/community'
    files = {directory+'/'+name:ROOT/directory/name
             for directory,names in SOURCES.items() for name in names}
    files['docs/guides/STRUCTURE.md'] = ROOT/'docs/guides/STRUCTURE.md'
    files['.gitignore'] = ROOT/'.gitignore'
    for name in ['README.md','BUILD.md','CONTRIBUTING.md','LICENSES.md','STATUS.json',
                 'licenses/GPL-3.0.txt','licenses/Linux-COPYING.txt']:
        files['docs/community/'+name] = docs/name
    manifest = {name:digest(path) for name,path in files.items()}
    # Public source must never grow from suffix-based directory scanning.
    require(all('reset-token.h' not in name and '.private' not in name for name in files), 'Private source member')
    require(not output.exists(), 'Existing release retained; output must be new')
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('xb') as stream, tarfile.open(fileobj=stream,mode='w:gz') as archive:
        for name,path in sorted(files.items()):
            info = archive.gettarinfo(str(path), arcname='kindle-native/'+name)
            info.uid = info.gid = 0
            info.uname = info.gname = ''
            info.mode = 0o755 if path.suffix in {'.py','.sh'} else 0o644
            with path.open('rb') as source:
                archive.addfile(info, source)
        data = (json.dumps(manifest,indent=2)+'\n').encode()
        info = tarfile.TarInfo('kindle-native/SOURCE-SHA256.json')
        info.size = len(data)
        info.mode = 0o644
        archive.addfile(info, io.BytesIO(data))
    print('Public-source candidate:',output,'files:',len(files),'sha256:',digest(output))
    print('No upload performed. Source checks do not certify a fresh physical first installation.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path, nargs='?', default=ROOT/'dist/pw3-source.tar.gz')
    args = parser.parse_args()
    try:
        package(args.output)
    except (ValueError, OSError, tarfile.TarError) as error:
        parser.exit(1,str(error)+'\n')


if __name__ == '__main__':
    main()
