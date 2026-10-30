#!/bin/sh
# Display failures remain independent of the working USB administration console.
set -eu
bb=/bin/busybox
compat=/opt/reader-compat
reader=/opt/koreader
export LD_LIBRARY_PATH="$compat:$reader/libs"
export HOME=/root LANG=C.UTF-8 LC_ALL=C.UTF-8
$bb mkdir -p /dev/input /var/tmp
[ -e /dev/fb0 ] || $bb mknod /dev/fb0 c 29 0
# Refresh immediately, before touchscreen setup, so boot is never silent.
cd "$reader"
"$compat/ld-linux.so.3" --library-path "$LD_LIBRARY_PATH" ./fbink -f -W GC16 -c -m -y 6 'Alpine Linux: starting reader...' 
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
done
[ -e /dev/urandom ] || $bb mknod /dev/urandom c 1 9
[ -e /dev/random ] || $bb mknod /dev/random c 1 8
[ -e /dev/zero ] || $bb mknod /dev/zero c 1 5
if [ -f /sys/class/rtc/rtc0/dev ] && [ ! -e /dev/rtc0 ]; then
    numbers=$($bb cat /sys/class/rtc/rtc0/dev)
    $bb mknod /dev/rtc0 c "${numbers%:*}" "${numbers#*:}"
fi
[ -e /dev/ptmx ] || $bb mknod /dev/ptmx c 5 2
# No Amazon launcher, framework or power daemon is started.
cd "$reader"
echo GW_NATIVE_DISPLAY_START
exec "$compat/ld-linux.so.3" --library-path "$LD_LIBRARY_PATH" ./luajit native.lua /mnt/us/documents
