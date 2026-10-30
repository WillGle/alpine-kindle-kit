#!/bin/sh
# Host-only freestanding ARM userspace build, no device command.
set -eu
base=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cross="$base/../toolchain-gcc-4.9.4/gcc-4.9.4-nolibc/arm-linux-gnueabi/bin/arm-linux-gnueabi-"
export LD_LIBRARY_PATH="$base/../abi-probe/host-prefix/lib:/nix/store/7n5v17xplf71jkc7km6kk6midfzmd3s7-gmp-6.3.0/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
"${cross}gcc" -Os -Wall -Wextra -Werror -Wno-unused-function -marm -march=armv7-a -mfloat-abi=soft -fno-builtin -fno-stack-protector -fno-pic -nostdlib -static -Wl,-e,_start -Wl,--build-id=none "$base/trace.c" -o "$base/trace" > "$base/build.log" 2>&1
"${cross}objdump" -dr "$base/trace" > "$base/trace.disassembly.txt"
test -z "$("${cross}nm" -u "$base/trace")"
echo 'Static ARM exec/rollback helper built; PID1 entry requires separately armed guardian.'
"${cross}gcc" -Os -Wall -Wextra -Werror -Wno-unused-function -marm -march=armv7-a -mfloat-abi=soft -fno-builtin -fno-stack-protector -fno-pic -nostdlib -static -Wl,-e,_start -Wl,--build-id=none "$base/bootstrap.c" -o "$base/bootstrap" >> "$base/build.log" 2>&1
test -z "$("${cross}nm" -u "$base/bootstrap")"
"${cross}gcc" -Os -Wall -Wextra -Werror -Wno-unused-function -marm -march=armv7-a -mfloat-abi=soft -fno-builtin -fno-stack-protector -fno-pic -nostdlib -static -Wl,-e,_start -Wl,--build-id=none "$base/guard-control.c" -o "$base/guard-control" >> "$base/build.log" 2>&1
test -z "$("${cross}nm" -u "$base/guard-control")"
src="$base/../linux-stock-5.16.2.1.1"
/nix/store/0xx06whymh7q41z839xvign6pblmb1x0-gnumake-4.4.1/bin/make \
    -C "$base/../abi-probe/build" KBUILD_SRC="$src" -f "$src/Makefile" ARCH=arm \
    CROSS_COMPILE="$cross" HOSTCC=/nix/store/lrn8jgbid9f9sdyznfrd5mm3bcmd4hw2-clang-wrapper-21.1.7/bin/clang \
    M="$base" V=1 modules >> "$base/build.log" 2>&1
