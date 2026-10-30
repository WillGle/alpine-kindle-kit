#!/bin/sh
set -eu
base=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cross="$base/../toolchain-gcc-4.9.4/gcc-4.9.4-nolibc/arm-linux-gnueabi/bin/arm-linux-gnueabi-"
LD_LIBRARY_PATH="$base/../abi-probe/host-prefix/lib:/nix/store/7n5v17xplf71jkc7km6kk6midfzmd3s7-gmp-6.3.0/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LD_LIBRARY_PATH
for program in init watchdog-read; do
    target=$program
    [ "$program" != watchdog-read ] || target=gleworks-watchdog-read
    "${cross}gcc" -Os -Wall -Wextra -Werror -marm -march=armv7-a -mfloat-abi=soft \
        -fno-builtin -fno-stack-protector -fno-pic -nostdlib -static \
        -Wl,-e,_start -Wl,--build-id=none "$base/$program.c" -o "$base/$target"
done
cat > "$base/initramfs.list" <<EOF
dir /dev 0755 0 0
dir /proc 0755 0 0
dir /sys 0755 0 0
nod /dev/console 0600 0 0 c 5 1
file /init $base/init 0755 0 0
EOF
