#!/bin/sh
# Compile diagnostic research artifacts on this Nix host; no device access.
set -eu
base=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
src="$base/../linux-6.18"
expected=7d0a66e4bb9081d75c82ec4957c50034cb0ea449
[ "$(git -C "$src" rev-parse HEAD)" = "$expected" ] || exit 1
[ -z "$(git -C "$src" status --porcelain --untracked-files=no)" ] || exit 1
# Existing local tools; target compiler bypasses Nix's host-libc wrapper.
clang=/nix/store/1qxqw8s8ccwxk2m47k9dvdbqqb0qlcpm-clang-21.1.7/bin/clang
linker=/nix/store/y5gy55c4az2g89gh4x0x1v4qbnnip6hs-llvm-binutils-22.0.0-rocm/bin/ld.lld
PATH=/nix/store/lrn8jgbid9f9sdyznfrd5mm3bcmd4hw2-clang-wrapper-21.1.7/bin:/nix/store/7y7p3y9fwvfwvwd2lhvl5iksi50vvgqv-llvm-binutils-wrapper-22.0.0-rocm/bin:/nix/store/2i1v8rlyvl0lhh6p4qx1ksj0g4r02j6c-llvm-21.1.7/bin:/nix/store/0xx06whymh7q41z839xvign6pblmb1x0-gnumake-4.4.1/bin:/nix/store/z5shcr7ysjqs7sgb4xrw8zp30p7k1iwx-bison-3.8.2/bin:/nix/store/mkrwg4hn1ckr1baqd70zvmicx04y31x9-flex-2.6.4/bin:/nix/store/q3mgh5nda6cwrlwg17daw81l0001w5sf-bc-1.08.2/bin:$PATH
export PATH
mkdir -p "$base/build"
sh "$base/build-init.sh"
{
    "$clang" --version
    "$linker" --version
    llvm-objcopy --version
} > "$base/toolchain.txt"
KCONFIG_ALLCONFIG="$base/probe.config" make -C "$src" O="$base/build" ARCH=arm LLVM=1 CC="$clang" LD="$linker" allnoconfig > "$base/configure.log" 2>&1
# Assert key dependencies survived Kconfig resolution.
for option in MMU SOC_IMX6SL ARM_APPENDED_DTB CMDLINE_FORCE USB_G_SERIAL; do
    grep -qx "CONFIG_$option=y" "$base/build/.config"
done
for option in MMC MTD NET DRM FB PM CPU_IDLE CPU_FREQ ARM_ATAG_DTB_COMPAT; do
    grep -qx "# CONFIG_$option is not set" "$base/build/.config"
done
make -C "$src" O="$base/build" ARCH=arm LLVM=1 CC="$clang" LD="$linker" -j4 zImage scripts_dtc > "$base/build.log" 2>&1
"$clang" -E -nostdinc -undef -D__DTS__ -x assembler-with-cpp \
    -I "$src/arch/arm/boot/dts/nxp/imx" -I "$src/scripts/dtc/include-prefixes" \
    "$base/imx6sl-pw3-probe.dts" -o "$base/build/imx6sl-pw3-probe.preprocessed.dts"
"$base/build/scripts/dtc/dtc" -I dts -O dtb -o "$base/imx6sl-pw3-probe.dtb" \
    "$base/build/imx6sl-pw3-probe.preprocessed.dts" > "$base/dtc.log" 2>&1
cd "$base"
python3 - <<'PY'
from pathlib import Path
import struct
for name in ['build/vmlinux', 'build/arch/arm/boot/compressed/vmlinux']:
    data = Path(name).read_bytes()
    assert data[:6] == b'\x7fELF\x01\x01', name
    assert struct.unpack_from('<H', data, 18)[0] == 40, name
    phoff = struct.unpack_from('<I', data, 28)[0]
    stride, count = struct.unpack_from('<HH', data, 42)
    kinds = [struct.unpack_from('<I', data, phoff + i * stride)[0] for i in range(count)]
    assert 2 not in kinds and 3 not in kinds, 'Dynamic/interpreter segment in ' + name
data = Path('build/arch/arm/boot/zImage').read_bytes()
magic, start, end = struct.unpack_from('<III', data, 36)
assert magic == 0x016f2818 and end - start == len(data)
data = Path('imx6sl-pw3-probe.dtb').read_bytes()
magic, size = struct.unpack_from('>II', data)
assert magic == 0xd00dfeed and size == len(data)
print('ARM ELF, static linkage, zImage and DTB structural checks passed.')
PY
sha256sum build/arch/arm/boot/zImage imx6sl-pw3-probe.dtb build/.config > SHA256SUMS
python3 "$base/package.py"
echo 'Host kernel/DTB build complete. Embedded diagnostic initramfs; not installed or hardware-validated.'
