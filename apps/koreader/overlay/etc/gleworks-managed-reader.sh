#!/bin/sh
# Experimental foreground launch; not used by the qualified reader boot.
set -eu
compat=/opt/reader-compat
reader=/opt/koreader
. /etc/gleworks-device-prepare.sh
: "${GW_TOUCH_DEV:=$(find_touch_node || true)}"
export GW_TOUCH_DEV
[ -c /dev/fb0 ] && [ -n "$GW_TOUCH_DEV" ] && [ -c "$GW_TOUCH_DEV" ] && [ -x /usr/sbin/gwctl ]
export LD_LIBRARY_PATH="$compat:$reader/libs"
export HOME=/root LANG=C.UTF-8 LC_ALL=C.UTF-8 GW_MANAGED_READER=1
cd "$reader"
exec "$compat/ld-linux.so.3" --library-path "$LD_LIBRARY_PATH" ./luajit native.lua /mnt/us/documents
