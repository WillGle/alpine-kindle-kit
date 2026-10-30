#!/bin/sh
set -eu
base=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cross="$base/toolchain-gcc-4.9.4/gcc-4.9.4-nolibc/arm-linux-gnueabi/bin/arm-linux-gnueabi-"
export LD_LIBRARY_PATH="$base/abi-probe/host-prefix/lib:/nix/store/7n5v17xplf71jkc7km6kk6midfzmd3s7-gmp-6.3.0/lib"
musl="$base/musl-arm-prefix"
for name in bootstrap shutdown; do
    "${cross}gcc" -std=gnu11 -Os -marm -march=armv7-a -mfloat-abi=soft -Wall -Wextra -Werror -nostdinc -isystem "$musl/include" -idirafter /nix/store/0xlb57h8i6jjzx4k44z363ig29yxkwya-linux-headers-6.18.7/include -nostdlib -static "$musl/lib/crt1.o" "$musl/lib/crti.o" "$base/bootstrap/$name.c" -Wl,--start-group "$musl/lib/libc.a" "$("${cross}gcc" -print-libgcc-file-name)" -Wl,--end-group "$musl/lib/crtn.o" -o "$base/bootstrap/$name-HOST-ONLY"
done
