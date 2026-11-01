#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Prepare only the existing PW3 framebuffer and touch modules for the shell.
set -eu
bb=/bin/busybox
$bb mkdir -p /dev/input /var/tmp
[ -e /dev/fb0 ] || $bb mknod /dev/fb0 c 29 0
$bb chmod 600 /dev/fb0
for name in cyttsp4_i2c cyttsp4_core cyttsp4_mt_b; do
    if ! $bb grep -q "^$name " /proc/modules; then
        $bb insmod "/lib/modules/3.0.35-lab126/kernel/drivers/input/touchscreen/$name.ko"
    fi
done
for input in /sys/class/input/event*; do
    [ -f "$input/dev" ] || continue
    numbers=$($bb cat "$input/dev")
    node=/dev/input/${input##*/}
    [ -e "$node" ] || $bb mknod "$node" c "${numbers%:*}" "${numbers#*:}"
    $bb chmod 600 "$node"
done
[ -e /dev/urandom ] || $bb mknod /dev/urandom c 1 9
[ -e /dev/random ] || $bb mknod /dev/random c 1 8
[ -e /dev/zero ] || $bb mknod /dev/zero c 1 5
$bb chmod 644 /dev/urandom /dev/random
$bb chmod 666 /dev/zero
if [ -f /sys/class/rtc/rtc0/dev ] && [ ! -e /dev/rtc0 ]; then
    numbers=$($bb cat /sys/class/rtc/rtc0/dev)
    $bb mknod /dev/rtc0 c "${numbers%:*}" "${numbers#*:}"
fi
[ -e /dev/rtc0 ] && $bb chmod 600 /dev/rtc0
[ -e /dev/ptmx ] || $bb mknod /dev/ptmx c 5 2
