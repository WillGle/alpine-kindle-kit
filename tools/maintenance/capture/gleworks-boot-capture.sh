#!/bin/sh
# Name: GleWorks Boot Capture
# Author: GleWorks
# Copy boot evidence to USB storage without changing boot state or raw devices.
set -eu
if [ "$(id -u)" != 0 ]; then
    exec /var/local/kmc/bin/su -c '/bin/sh /mnt/us/documents/gleworks-boot-capture.sh'
fi
umask 077
[ "$(cat /sys/class/block/mmcblk0p1/start)" = 65536 ] || exit 1
[ "$(cat /sys/class/block/mmcblk0boot0/size)" = 8192 ] || exit 1
[ "$(cat /sys/class/block/mmcblk0boot1/size)" = 8192 ] || exit 1
command -v sha256sum >/dev/null
free_kb=$(df -k /mnt/us | awk 'END {print $4}')
case "$free_kb" in ''|*[!0-9]*) exit 1;; esac
[ "$free_kb" -ge 131072 ] || { echo 'Need 128 MiB free on user storage'; exit 1; }
out=/mnt/us/gleworks-boot-capture-$(date +%Y%m%d-%H%M%S)-$$
mkdir "$out"
exec >"$out/capture.log" 2>&1
trap 'echo "Capture interrupted or incomplete; no boot state changed"; exit 1' HUP INT TERM
capture() {
    device=$1
    name=$2
    blocks=$3
    expected=$4
    # All outputs are ordinary files inside the newly created directory.
    dd if="$device" of="$out/$name" bs=65536 count="$blocks"
    actual=$(wc -c < "$out/$name")
    [ "$actual" -eq "$expected" ] || { echo "Short read: $name"; exit 1; }
}
echo 'Private boot evidence; not a complete recovery backup.'
date -u
uname -srvm
capture /dev/mmcblk0boot0 boot0.bin 64 4194304
capture /dev/mmcblk0boot1 boot1.bin 64 4194304
capture /dev/mmcblk0 pre-partition.bin 512 33554432
# Preserve the userspace idme implementation for static inspection, never execute it.
if [ -f /usr/sbin/idme ]; then cp /usr/sbin/idme "$out/idme"; fi
cd "$out"
sha256sum boot0.bin boot1.bin pre-partition.bin > SHA256SUMS
if [ -f idme ]; then sha256sum idme >> SHA256SUMS; fi
sha256sum -c SHA256SUMS
# Marker is written last; incomplete captures must not be used.
echo 'COMPLETE: all requested bytes copied; local file hashes verified.' > COMPLETE.txt
echo 'No idme execution, reboot, raw-device write, firmware or partition change.'
