#!/bin/sh
# Compile objects/blob only; no .ko, module load, device or installation.
set -eu
base=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
abi="$base/../abi-probe"
src="$base/../linux-stock-5.16.2.1.1"
cross="$base/../toolchain-gcc-4.9.4/gcc-4.9.4-nolibc/arm-linux-gnueabi/bin/arm-linux-gnueabi-"
LD_LIBRARY_PATH="$abi/host-prefix/lib:/nix/store/7n5v17xplf71jkc7km6kk6midfzmd3s7-gmp-6.3.0/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LD_LIBRARY_PATH
/nix/store/0xx06whymh7q41z839xvign6pblmb1x0-gnumake-4.4.1/bin/make \
    -C "$abi/build" KBUILD_SRC="$src" -f "$src/Makefile" ARCH=arm \
    CROSS_COMPILE="$cross" \
    HOSTCC=/nix/store/lrn8jgbid9f9sdyznfrd5mm3bcmd4hw2-clang-wrapper-21.1.7/bin/clang \
    M="$base" V=1 transition.o chain.o > "$base/build.log" 2>&1
"${cross}gcc" -c -marm -march=armv7-a "$base/relocate.S" -o "$base/relocate.o"
"${cross}objcopy" -O binary --only-section=.text.gw_relocate "$base/relocate.o" "$base/relocate.bin"
"${cross}objdump" -dr "$base/relocate.o" > "$base/relocate.disassembly.txt"
"${cross}objdump" -dr "$base/transition.o" > "$base/transition.disassembly.txt"
echo 'Compile-only handoff candidate built; no loadable module or device command.'
