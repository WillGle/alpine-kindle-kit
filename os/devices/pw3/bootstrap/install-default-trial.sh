#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Install only the stock-default dispatcher; never create either boot request.
set -eu
umask 077
stage=$1
original=ad624cbad3090bf15e65c8420a4e451ee10fd067a40b1bfe662965e82d932d0a
trial=/gleworks-native-trial
[ "$(id -u)" = 0 ]
[ "$(uname -r)" = 3.0.35-lab126 ]
[ "$(readlink /proc/1/exe)" = /sbin/init.exe ]
[ -f /sbin/init ] && [ ! -L /sbin/init ]
[ "$(sha256sum /sbin/init | awk '{print $1}')" = "$original" ]
for directory in "$trial" /modules /control /newroot /sbin/init.gleworks-stock /sbin/init.gleworks-new /sbin/init.gleworks-restore /etc/upstart/gleworks-coldboot-control.conf /var/local/gleworks-coldboot-control.once; do
    [ ! -e "$directory" ] && [ ! -L "$directory" ]
done
cd "$stage"
sha256sum -c SHA256SUMS
/bin/sh -n dispatcher.sh
./bootstrap --check
cleanup() {
    result=$?
    trap - EXIT HUP INT TERM
    /bin/sync
    /bin/mount -o remount,ro / || exit 1
    exit "$result"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
/bin/mount -o remount,rw /
# Preserve the known wrapper before creating the replacement inode.
cp -p /sbin/init /sbin/init.gleworks-stock
/bin/sync
[ "$(sha256sum /sbin/init.gleworks-stock | awk '{print $1}')" = "$original" ]
mkdir "$trial" /modules /control /newroot
chmod 755 "$trial" /modules /control /newroot
cp bootstrap "$trial/bootstrap"
chmod 755 "$trial/bootstrap"
for name in fsl_otg_arc arcotg_udc g_file_storage gleworks_pid1_reset; do
    cp "$name.ko" "/modules/$name.ko"
    chmod 600 "/modules/$name.ko"
    [ "$(sha256sum "$name.ko" | awk '{print $1}')" = "$(sha256sum "/modules/$name.ko" | awk '{print $1}')" ]
done
[ "$(sha256sum bootstrap | awk '{print $1}')" = "$(sha256sum "$trial/bootstrap" | awk '{print $1}')" ]
cp dispatcher.sh /sbin/init.gleworks-new
chmod 755 /sbin/init.gleworks-new
cp return-control.conf /etc/upstart/gleworks-coldboot-control.conf
chmod 644 /etc/upstart/gleworks-coldboot-control.conf
[ "$(sha256sum return-control.conf | awk '{print $1}')" = "$(sha256sum /etc/upstart/gleworks-coldboot-control.conf | awk '{print $1}')" ]
/bin/sync
[ "$(sha256sum dispatcher.sh | awk '{print $1}')" = "$(sha256sum /sbin/init.gleworks-new | awk '{print $1}')" ]
[ ! -e "$trial/try.once" ]
/bin/mv /sbin/init.gleworks-new /sbin/init
/bin/sync
[ "$(sha256sum dispatcher.sh | awk '{print $1}')" = "$(sha256sum /sbin/init | awk '{print $1}')" ]
echo GW_DEFAULT_DISPATCH_INSTALLED_NO_TRIAL_REQUEST
