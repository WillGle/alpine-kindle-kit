#!/bin/sh
# Display failures remain independent of the working USB administration console.
set -eu
bb=/bin/busybox
compat=/opt/reader-compat
reader=/opt/koreader
export LD_LIBRARY_PATH="$compat:$reader/libs"
export HOME=/root LANG=C.UTF-8 LC_ALL=C.UTF-8 GW_MANAGED_READER=1
. /etc/gleworks-device-prepare.sh
create_core_devices
# Refresh immediately, before touchscreen setup, so boot is never silent.
cd "$reader"
"$compat/ld-linux.so.3" --library-path "$LD_LIBRARY_PATH" ./fbink -f -W GC16 -c -m -y 6 'Alpine Linux: starting reader...' 
load_touch_modules
create_input_nodes
# Discover the touchscreen instead of assuming event1; extra input devices
# (uinput tests, USB keyboards) can shift the event number.
if touch_node=$(find_touch_node); then
    export GW_TOUCH_DEV="$touch_node"
    echo "GW_TOUCH_DEV=$GW_TOUCH_DEV"
else
    echo "GW_TOUCH_DEV not found" >&2
fi
# One lock watcher for the reader's lifetime: no gwctl spawn per poll.
if [ -x /usr/sbin/gwctl ] && ! pidof gwctl >/dev/null 2>&1; then
    /usr/sbin/gwctl watch >/run/gwctl-watch.log 2>&1 &
fi
# No Amazon launcher, framework or power daemon is started.
cd "$reader"
echo GW_NATIVE_DISPLAY_START
exec "$compat/ld-linux.so.3" --library-path "$LD_LIBRARY_PATH" ./luajit native.lua /mnt/us/documents
