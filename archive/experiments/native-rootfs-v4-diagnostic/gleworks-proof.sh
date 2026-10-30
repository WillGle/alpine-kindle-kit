#!/bin/sh
bb=/bin/busybox
[ -f /run/usb-ready ] || { $bb sleep 2; exit 1; }
while IFS= read -r challenge; do
    [ "${#challenge}" = 32 ] || continue
    case "$challenge" in *[!0-9a-f]*) continue;; esac
    [ "$($bb cat /etc/alpine-release)" = 3.24.3 ] || continue
    [ "$($bb readlink /proc/1/exe)" = /bin/busybox ] || continue
    printf 'GLEWORKS_ALPINE_NATIVE_ROOT release=3.24.3 kernel=%s pid1=%s challenge=%s\n' "$($bb uname -r)" "$($bb readlink /proc/1/exe)" "$challenge"
done
