#!/bin/sh
# Host-only build against captured stock configuration; never accesses Kindle.
set -eu
base=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
src="$base/../linux-stock-5.16.2.1.1"
cross="$base/../toolchain-gcc-4.9.4/gcc-4.9.4-nolibc/arm-linux-gnueabi/bin/arm-linux-gnueabi-"
make=/nix/store/0xx06whymh7q41z839xvign6pblmb1x0-gnumake-4.4.1/bin/make
hostcc=/nix/store/lrn8jgbid9f9sdyznfrd5mm3bcmd4hw2-clang-wrapper-21.1.7/bin/clang
test -f "$base/host-prefix/lib/libmpfr.so.4"
test -f "$base/host-prefix/lib/libmpc.so.3"
LD_LIBRARY_PATH="$base/host-prefix/lib:/nix/store/7n5v17xplf71jkc7km6kk6midfzmd3s7-gmp-6.3.0/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LD_LIBRARY_PATH
mkdir -p "$base/build"
cp "$base/../stock-kernel.config" "$base/build/.config"
"${cross}gcc" --version > "$base/toolchain.txt"
"${cross}gcc" -dumpmachine >> "$base/toolchain.txt"
# Enter the output directory directly: legacy O= recursion requires /bin/pwd,
# which this Nix host does not provide. The source tree stays untouched.
stock_make() {
    "$make" -C "$base/build" KBUILD_SRC="$src" -f "$src/Makefile" \
        ARCH=arm CROSS_COMPILE="$cross" HOSTCC="$hostcc" "$@"
}
stock_make oldconfig < /dev/null > "$base/configure.log" 2>&1
stock_make modules_prepare > "$base/prepare.log" 2>&1
stock_make M="$base" modules > "$base/build.log" 2>&1
sha256sum "$base/gleworks_abi_probe.ko" > "$base/module.sha256"
echo 'Host build complete; module not loaded or installed.'
