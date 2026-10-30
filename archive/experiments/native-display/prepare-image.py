#!/usr/bin/env python3
"""Prepare a display candidate from a clean, offline Alpine root; no boot writes."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess

base = Path(__file__).resolve().parent
import argparse
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--base-image', type=Path, required=True, help='Clean offline host backup, never the running device image')
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--enable-reader', action='store_true')
options = parser.parse_args()
original = options.base_image.resolve()
assert original.is_file() and not original.is_symlink()
image = options.output.resolve()
assert not image.exists(), 'Preserve existing artifacts; choose another output path'
image.parent.mkdir(parents=True, exist_ok=True)
shutil.copyfile(original, image)
image.chmod(0o600)
with image.open('r+b') as stream:
    stream.truncate(256*1024*1024)
for args in [['e2fsck','-fy',str(image)], ['resize2fs',str(image)]]:
    result = subprocess.run(args, capture_output=True, text=True)
    assert result.returncode in ([0,1] if args[0] == 'e2fsck' else [0]), result.stdout+result.stderr

overlay = base/'overlay'
shutil.copyfile(base.parent/'native-rootfs-persistent/gleworks-early.sh', overlay/'etc/gleworks-early.sh')
(overlay/'gleworks').mkdir(exist_ok=True)
shutil.copyfile(base.parent/'bootstrap/shutdown-HOST-ONLY', overlay/'gleworks/shutdown')
(overlay/'gleworks/shutdown').chmod(0o700)
for name in ['gleworks-display.sh','gleworks-reader-service.sh']:
    shutil.copyfile(base/name, overlay/'etc'/name)
    subprocess.run(['sh','-n',str(overlay/'etc'/name)], check=True)
shutil.copyfile(base/'native.lua', overlay/'opt/koreader/native.lua')
settings = overlay/'opt/koreader/settings.reader.lua'
settings.write_text('''return {
    ["language"] = "vi",
    ["home_dir"] = "/mnt/us/documents",
    ["lastdir"] = "/mnt/us/documents",
    ["first_run"] = false,
    ["dev_no_c_blitter"] = false,
    ["wifi_was_on"] = false,
    ["full_refresh_count"] = 1,
    ["night_full_refresh_count"] = 1,
}
''')
inittab = overlay/'etc/inittab'
inittab.write_text((base.parent/'native-rootfs-persistent/inittab').read_text() + '::respawn:/bin/busybox sh /etc/gleworks-reader-service.sh\n')
commands = ['rm /etc/gleworks-persistent-proof']
if options.enable_reader:
    enabled = image.parent/'reader-enabled'
    enabled.write_bytes(b'')
    commands += [f'write {enabled} /etc/gleworks-reader.enabled', 'set_inode_field /etc/gleworks-reader.enabled mode 0100644']
# Keep /mnt/us as a link into the one existing FAT mount, never a second mount.
commands += ['rmdir /mnt/us', 'symlink /mnt/us /boot/gleworks-control',
             'symlink /lib/ld-linux.so.3 /opt/reader-compat/ld-linux.so.3']
def included(path):
    parts = path.relative_to(overlay).parts
    # English source strings remain built in; retain Vietnamese and UK English.
    return not (len(parts) >= 4 and parts[:3] == ('opt', 'koreader', 'l10n')
                and parts[3] not in {'vi', 'en_GB', 'LICENSE', 'README.md'})

for path in sorted(filter(included, overlay.rglob('*')), key=lambda p:(len(p.parts), str(p))):
    rel = '/'+str(path.relative_to(overlay))
    assert ' ' not in rel and not path.is_symlink()
    if path.is_dir():
        commands.append('mkdir '+rel)
    else:
        # Only existing configuration is replaced; new app paths are absent.
        if rel in {'/etc/inittab', '/etc/gleworks-early.sh', '/gleworks/shutdown'}:
            commands.append('rm '+rel)
        commands.append(f'write {path} {rel}')
        commands.append('set_inode_field '+rel+' mode '+('0100755' if path.stat().st_mode & 0o111 else '0100644'))
    commands += ['set_inode_field '+rel+' uid 0', 'set_inode_field '+rel+' gid 0']
result = subprocess.run(['debugfs','-w',str(image)],input='\n'.join(commands)+'\n',text=True,capture_output=True,check=True)
image.with_suffix('.prepare.log').write_text(result.stdout+result.stderr)
assert 'Could not allocate' not in result.stderr and 'No space left' not in result.stderr
result = subprocess.run(['e2fsck','-fn',str(image)],capture_output=True,text=True)
image.with_suffix('.fsck.log').write_text(result.stdout+result.stderr)
assert result.returncode == 0,result.stdout+result.stderr
for path in ['/opt/koreader/luajit','/opt/koreader/native.lua','/etc/gleworks-display.sh']:
    result = subprocess.run(['debugfs','-R','stat '+path,str(image)],capture_output=True,text=True)
    assert 'Inode:' in result.stdout, result.stderr
review = {'scope':'Native Alpine display/KOReader candidate', 'reader_autostart':options.enable_reader,
          'bytes':image.stat().st_size,
          'base_sha256':hashlib.sha256(original.read_bytes()).hexdigest(),
          'sha256':hashlib.sha256(image.read_bytes()).hexdigest(),
          'reader_revision':(overlay/'opt/koreader/git-rev').read_text().strip(),
          'compatibility':'Generic glibc2.20 libraries from exact saved p1; no Amazon service binaries or Lua LIPC integration',
          'physical_verified':False,
          'input_sha256':{str(p.relative_to(overlay)):hashlib.sha256(p.read_bytes()).hexdigest() for p in overlay.rglob('*') if p.is_file() and included(p)}}
image.with_suffix('.review.json').write_text(json.dumps(review,indent=2)+'\n')
print('Clean 256MiB display candidate prepared:', image)
