#!/bin/sh
# Reuse prepared stock headers and private host libraries; no device access.
set -eu
base=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
abi="$base/../abi-probe"
src="$base/../linux-stock-5.16.2.1.1"
cross="$base/../toolchain-gcc-4.9.4/gcc-4.9.4-nolibc/arm-linux-gnueabi/bin/arm-linux-gnueabi-"
test -f "$abi/build/include/generated/autoconf.h"
LD_LIBRARY_PATH="$abi/host-prefix/lib:/nix/store/7n5v17xplf71jkc7km6kk6midfzmd3s7-gmp-6.3.0/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LD_LIBRARY_PATH
/nix/store/0xx06whymh7q41z839xvign6pblmb1x0-gnumake-4.4.1/bin/make \
    -C "$abi/build" KBUILD_SRC="$src" -f "$src/Makefile" ARCH=arm \
    CROSS_COMPILE="$cross" \
    HOSTCC=/nix/store/lrn8jgbid9f9sdyznfrd5mm3bcmd4hw2-clang-wrapper-21.1.7/bin/clang \
    M="$base" V=1 modules > "$base/build.log" 2>&1
(cd "$base" && sha256sum gleworks_kexec_load.ko) > "$base/module.sha256"
"${cross}gcc" -Os -Wall -Wextra -Werror -marm -march=armv7-a -mfloat-abi=soft \
    -fno-builtin -fno-stack-protector -fno-pic -nostdlib -static \
    -Wl,-e,_start -Wl,--build-id=none "$base/batch-helper.c" \
    -o "$base/gleworks-load-helper" > "$base/helper-build.log" 2>&1
echo 'Host load-only probe built; not loaded or installed.'
