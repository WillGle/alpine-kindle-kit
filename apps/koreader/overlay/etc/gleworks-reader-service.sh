#!/bin/sh
bb=/bin/busybox
# A failed reader leaves the USB console usable and avoids a crash loop.
[ -f /run/usb-ready ] && [ -f /etc/gleworks-reader.enabled ] || { $bb sleep 2; exit 1; }
[ -f /run/reader-stopped ] && { $bb sleep 30; exit 1; }
/bin/busybox sh /etc/gleworks-display.sh > /run/reader.log 2>&1
result=$?
if [ "$result" = 0 ] || [ "$result" = 85 ]; then
    # This appliance returns to its library after Quit or a requested restart.
    $bb sleep 2
    exit 0
fi
echo 'Reader failed; inspect reader.log over the USB console.' >&2
/opt/reader-compat/ld-linux.so.3 --library-path /opt/reader-compat:/opt/koreader/libs /opt/koreader/fbink -c -m -y 6 'Alpine is running. Reader failed.' 'Use USB console to inspect reader.log.' || :
$bb touch /run/reader-stopped
$bb sleep 30
