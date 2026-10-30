#!/bin/sh
# Promote the already-qualified PW3 bootstrap; never use before physical USB repair proof.
set -eu
bootstrap=$1
bootstrap_hash=$2
image_hash=$3
dispatch_hash=$4
trial=/gleworks-native-trial
[ "$(id -u)" = 0 ]
[ "$(uname -r)" = 3.0.35-lab126 ]
[ "$(readlink /proc/1/exe)" = /sbin/init.exe ]
[ "$(cat /sys/class/power_supply/max77696-battery/capacity)" -ge 50 ]
[ "$(cat /sys/class/block/mmcblk0p1/start)" = 65536 ]
[ "$(cat /sys/class/block/mmcblk0p1/size)" = 921600 ]
[ "$(sha256sum /sbin/init | awk '{print $1}')" = "$dispatch_hash" ]
[ "$(sha256sum /sbin/init.gleworks-stock | awk '{print $1}')" = ad624cbad3090bf15e65c8420a4e451ee10fd067a40b1bfe662965e82d932d0a ]
[ "$(sha256sum "$bootstrap" | awk '{print $1}')" = "$bootstrap_hash" ]
[ "$(sha256sum /mnt/us/gleworks-alpine/system.ext3 | awk '{print $1}')" = "$image_hash" ]
for path in "$trial/try.once" /mnt/us/gleworks-alpine/boot.once /mnt/us/gleworks-alpine/boot.pending /mnt/us/gleworks-alpine/boot.enabled /sbin/init.gleworks-dispatch /sbin/init.gleworks-native-new; do
    [ ! -e "$path" ] && [ ! -L "$path" ]
done
"$bootstrap" --check
umask 077
printf '%s\n' gleworks-alpine-v1 > /mnt/us/gleworks-alpine/boot.enabled
sync
trap 'sync; /bin/mount -o remount,ro /' EXIT
/bin/mount -o remount,rw /
cp -p /sbin/init /sbin/init.gleworks-dispatch
cp "$bootstrap" /sbin/init.gleworks-native-new
chmod 755 /sbin/init.gleworks-native-new
[ "$(sha256sum /sbin/init.gleworks-native-new | awk '{print $1}')" = "$bootstrap_hash" ]
sync
mv /sbin/init.gleworks-native-new /sbin/init
sync
/bin/mount -o remount,ro /
[ "$(sha256sum /sbin/init | awk '{print $1}')" = "$bootstrap_hash" ]
echo GW_NATIVE_ENTRY_INSTALLED_REBOOT_REQUIRED
