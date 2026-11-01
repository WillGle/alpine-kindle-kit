#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Reset inactive Amazon user state without invoking its whole-device reset.
set -eu
required=--erase-amazon-user-state
dry_run=0
confirmed=0
for arg in "$@"; do
    case "$arg" in
        "$required") ;;
        --dry-run) dry_run=1 ;;
        --yes) confirmed=1 ;;
        *) echo "usage: $0 $required [--dry-run] [--yes]" >&2; exit 2 ;;
    esac
done
[ "$dry_run" = 1 ] || [ "$confirmed" = 1 ] ||
    { echo "refusing destructive cleanup without --yes (use --dry-run to inspect)" >&2; exit 2; }
[ "$(readlink /proc/1/exe)" = /bin/busybox ]
[ "$(cat /etc/alpine-release)" = 3.24.3 ]
[ "$(cat /sys/block/mmcblk0/mmcblk0p1/size)" = 921600 ]
[ "$(cat /sys/block/mmcblk0/mmcblk0p3/size)" = 131072 ]
/gleworks/guard-control status | grep -q '^state=00000000$'
! grep -q '^/dev/mmcblk0p[13] ' /proc/mounts
stock=/run/gleworks-clean-stock
local=/run/gleworks-clean-local
mkdir "$stock" "$local"
finish() {
    sync
    if grep -q " $local " /proc/mounts; then umount "$local" || exit 1; fi
    if grep -q " $stock " /proc/mounts; then umount "$stock" || exit 1; fi
    rmdir "$local" "$stock"
}
trap finish EXIT
trap 'exit 1' HUP INT TERM
for part in 1 3; do
    [ -b /dev/mmcblk0p$part ] || mknod /dev/mmcblk0p$part b 179 "$part"
done
mount -t ext2 -o ro /dev/mmcblk0p1 "$stock"
[ "$(sha256sum "$stock/sbin/init" | awk '{print $1}')" = b4aa162ffc2972a59a3c649d15b600c90e495e5025f6c29bada7798acb0b7175 ]
[ "$(sha256sum "$stock/sbin/init.gleworks-stock" | awk '{print $1}')" = ad624cbad3090bf15e65c8420a4e451ee10fd067a40b1bfe662965e82d932d0a ]
[ -d "$stock/opt/var/local" ]
[ -f "$stock/opt/amazon/factory_fresh/factory_fresh" ]
mount -t ext3 -o rw /dev/mmcblk0p3 "$local"
# Inventory every top-level object before touching it.
inventory_bytes=$(du -sk "$local" 2>/dev/null | awk '{print $1*1024}')
inventory_entries=$(find "$local" -mindepth 1 -maxdepth 1 | wc -l)
printf 'GLEWORKS_CLEAN_INVENTORY entries=%s bytes=%s\n' "$inventory_entries" "$inventory_bytes"
if [ "$dry_run" = 1 ]; then
    echo GLEWORKS_CLEAN_DRY_RUN
    exit 0
fi
[ -d "$local/eink/waveforms" ] && [ ! -L "$local/eink" ]
find "$local/eink" -type f -exec sha256sum '{}' \; | sort > /run/amazon-eink-before.sha256
# Stage defaults first, so a failed copy never deletes the existing state.
new="$local/.gleworks-factory-new"
if [ -e "$new" ] || [ -L "$new" ]; then
    [ -d "$new" ] && [ ! -L "$new" ]
    rm -rf -- "$new"
fi
mkdir "$new"
cp -a "$stock/opt/var/local/." "$new/"
mkdir -p "$new/system"
cp -a "$stock/opt/amazon/factory_fresh/." "$new/system/"
if [ -f "$stock/etc/guid" ]; then cp -p "$stock/etc/guid" "$new/guid"; fi
sync
for path in "$local"/* "$local"/.[!.]* "$local"/..?*; do
    [ -e "$path" ] || [ -L "$path" ] || continue
    case "$path" in "$local/eink"|"$new") continue ;; esac
    # The old jailbreak protected its payload with immutable inode flags.
    if [ ! -L "$path" ]; then
        find "$path" -xdev ! -type l -exec chattr -ia '{}' +
    fi
    rm -rf -- "$path"
done
cp -a "$new/." "$local/"
rm -rf -- "$new"
chown 9000:150 "$local"
chmod 2775 "$local"
find "$local/eink" -type f -exec sha256sum '{}' \; | sort > /run/amazon-eink-after.sha256
cmp /run/amazon-eink-before.sha256 /run/amazon-eink-after.sha256
test -f "$local/system/factory_fresh"
test -f "$local/system/factory_fresh_authentication"
test ! -e "$local/reg"
test ! -e "$local/java"
test ! -e "$local/kmc"
test ! -e "$local/system/wifilocker"
echo AMAZON_FACTORY_STATE_SEEDED
du -sk "$local"
sha256sum "$stock/sbin/init" "$stock/sbin/init.gleworks-stock"
echo EINK_WAVEFORMS_UNCHANGED
