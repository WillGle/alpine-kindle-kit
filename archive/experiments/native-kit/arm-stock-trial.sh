#!/bin/sh
# One-shot readonly Alpine coldboot with the already-qualified 90-second guardian.
set -eu
bootstrap_hash=$1
image_hash=$2
dispatch_hash=$3
[ "$(readlink /proc/1/exe)" = /sbin/init.exe ]
[ "$(uname -r)" = 3.0.35-lab126 ]
[ "$(cat /sys/class/power_supply/max77696-battery/capacity)" -ge 50 ]
[ "$(sha256sum /sbin/init | awk '{print $1}')" = "$dispatch_hash" ]
[ "$(sha256sum /gleworks-native-trial/bootstrap | awk '{print $1}')" = "$bootstrap_hash" ]
[ "$(sha256sum /mnt/us/gleworks-alpine/system.ext3 | awk '{print $1}')" = "$image_hash" ]
for path in /gleworks-native-trial/try.once /mnt/us/gleworks-alpine/boot.once /mnt/us/gleworks-alpine/boot.pending /mnt/us/gleworks-alpine/boot.enabled /var/local/gleworks-coldboot-control.once; do
    [ ! -e "$path" ] && [ ! -L "$path" ]
done
umask 077
printf '%s\n' gleworks-alpine-v1 > /mnt/us/gleworks-alpine/boot.once
printf '%s\n' gleworks-coldboot-control-v1 > /var/local/gleworks-coldboot-control.once
sync
trap 'sync; /bin/mount -o remount,ro /' EXIT
/bin/mount -o remount,rw /
printf '%s\n' 'one supervised coldboot only' > /gleworks-native-trial/try.once
sync
/bin/mount -o remount,ro /
echo GW_READONLY_NATIVE_TRIAL_ARMED
/sbin/reboot
