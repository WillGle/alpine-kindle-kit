#!/bin/sh
set -eu
base=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cross="$base/toolchain-gcc-4.9.4/gcc-4.9.4-nolibc/arm-linux-gnueabi/bin/arm-linux-gnueabi-"
export LD_LIBRARY_PATH="$base/abi-probe/host-prefix/lib:/nix/store/7n5v17xplf71jkc7km6kk6midfzmd3s7-gmp-6.3.0/lib"
cd "$base/musl-arm-build"
CC="${cross}gcc" AR="${cross}ar" RANLIB="${cross}ranlib" CFLAGS='-Os -marm -march=armv7-a -mfloat-abi=soft' ../musl-1.2.5-source/configure --target=arm-linux-musleabi --prefix="$base/musl-arm-prefix" --disable-shared > configure.log 2>&1
/nix/store/0xx06whymh7q41z839xvign6pblmb1x0-gnumake-4.4.1/bin/make -j4 install > build.log 2>&1
