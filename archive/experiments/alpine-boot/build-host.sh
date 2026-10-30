#!/bin/sh
# Independent host build; preserve the device-qualified diagnostic image.
set -eu
base=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
src="$base/../linux-6.18"
[ "$(git -C "$src" rev-parse HEAD)" = 7d0a66e4bb9081d75c82ec4957c50034cb0ea449 ]
[ -z "$(git -C "$src" status --porcelain --untracked-files=no)" ]
python3 "$base/prepare-initramfs.py"
clang=/nix/store/1qxqw8s8ccwxk2m47k9dvdbqqb0qlcpm-clang-21.1.7/bin/clang
linker=/nix/store/y5gy55c4az2g89gh4x0x1v4qbnnip6hs-llvm-binutils-22.0.0-rocm/bin/ld.lld
PATH=/nix/store/lrn8jgbid9f9sdyznfrd5mm3bcmd4hw2-clang-wrapper-21.1.7/bin:/nix/store/7y7p3y9fwvfwvwd2lhvl5iksi50vvgqv-llvm-binutils-wrapper-22.0.0-rocm/bin:/nix/store/2i1v8rlyvl0lhh6p4qx1ksj0g4r02j6c-llvm-21.1.7/bin:/nix/store/0xx06whymh7q41z839xvign6pblmb1x0-gnumake-4.4.1/bin:/nix/store/z5shcr7ysjqs7sgb4xrw8zp30p7k1iwx-bison-3.8.2/bin:/nix/store/mkrwg4hn1ckr1baqd70zvmicx04y31x9-flex-2.6.4/bin:/nix/store/q3mgh5nda6cwrlwg17daw81l0001w5sf-bc-1.08.2/bin:$PATH
export PATH
mkdir -p "$base/build"
python3 - "$base" <<'PY'
from pathlib import Path
import sys,re
base=Path(sys.argv[1]);s=(base.parent/'native-boot/build/.config').read_text()
s=re.sub(r'^CONFIG_INITRAMFS_SOURCE=.*$', 'CONFIG_INITRAMFS_SOURCE="'+str(base/'initramfs.list')+'"',s,flags=re.M)
s=s.replace('panic=0\"', 'panic=0 nohlt\"')
s=re.sub(r'^CONFIG_LOCALVERSION=.*$', 'CONFIG_LOCALVERSION="-gleworks-alpine-probe"',s,flags=re.M)
(base/'build/.config').write_text(s)
PY
make -C "$src" O="$base/build" ARCH=arm LLVM=1 CC="$clang" LD="$linker" olddefconfig > "$base/configure.log" 2>&1
for option in MMC MTD NET DRM FB PM WATCHDOG MODULES; do
    grep -qx "# CONFIG_$option is not set" "$base/build/.config"
done
for option in USB_G_SERIAL BLK_DEV_INITRD DEVTMPFS BINFMT_ELF VFP; do
    grep -qx "CONFIG_$option=y" "$base/build/.config"
done
make -C "$src" O="$base/build" ARCH=arm LLVM=1 CC="$clang" LD="$linker" -j4 zImage > "$base/build.log" 2>&1
echo 'Independent Alpine initramfs kernel compiled on host; no device operation.'
