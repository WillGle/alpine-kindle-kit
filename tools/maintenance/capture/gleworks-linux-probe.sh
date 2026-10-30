#!/bin/sh
# Name: GleWorks Linux Hardware Probe
# Author: GleWorks
# Existing root helper is used only to read metadata; only the report is written.
set -u
if [ "$(id -u)" != 0 ]; then
    [ -x /var/local/kmc/bin/su ] || { echo 'Existing KMC root helper unavailable'; exit 1; }
    exec /var/local/kmc/bin/su -c '/bin/sh /mnt/us/documents/gleworks-linux-probe.sh'
fi
[ -d /mnt/us/documents ] || exit 1
umask 077
set -C
report=/mnt/us/gleworks-linux-probe-$(date +%Y%m%d-%H%M%S)-$$.txt
exec >"$report" 2>&1 || exit 1
section() { printf '\n--- %s ---\n' "$1"; }
readfile() { for f do if [ -r "$f" ]; then printf '\n%s\n' "$f"; cat "$f"; fi; done; }
echo 'Read-only Linux conversion probe v2; no raw partitions or credentials dumped.'
date -u
section 'Kernel and CPU'
uname -srvm
grep -E '^(Processor|model name|CPU architecture|CPU implementer|CPU part|Features|Hardware)[[:space:]]*:' /proc/cpuinfo
readfile /proc/meminfo /proc/partitions /proc/mtd /proc/filesystems
section 'Selected boot arguments'
# Allowlist avoids serial numbers or identifiers in the complete command line.
tr ' ' '\n' < /proc/cmdline | grep -E '^(root=|rootfstype=|rootwait$|ro$|rw$|console=|video=|mem=|init=|rdinit=|bootmode=)'
section 'Mounts and free space'
awk '$2 == "/" || $2 == "/mnt/us" || $2 == "/var/local" {print}' /proc/mounts
df -k / /mnt/us /var/local
section 'eMMC geometry (sector units are 512 bytes)'
for d in /sys/class/block/mmcblk*; do
    [ -e "$d" ] || continue
    echo "$d"
    readfile "$d/size" "$d/start" "$d/ro" "$d/partition" "$d/queue/logical_block_size" "$d/device/name" "$d/device/type" "$d/device/manfid" "$d/device/oemid"
done
section 'Loaded modules and hardware drivers'
readfile /proc/modules /proc/fb
for bus in platform i2c spi sdio usb; do
    for d in /sys/bus/$bus/devices/*; do
        [ -e "$d" ] || continue
        echo "$d"
        readlink "$d/driver" 2>/dev/null || :
        readfile "$d/modalias" "$d/vendor" "$d/device" "$d/idVendor" "$d/idProduct"
    done
done
section 'Input devices (identifiers omitted)'
grep -E '^(I:|N:|H:|B:)' /proc/bus/input/devices
section 'Display and frontlight metadata'
for d in /sys/class/graphics/fb* /sys/class/backlight/* /sys/class/leds/*; do
    [ -e "$d" ] || continue
    readfile "$d/name" "$d/virtual_size" "$d/bits_per_pixel" "$d/modes" "$d/max_brightness" "$d/brightness"
done
section 'Power capabilities (no suspend triggered)'
readfile /sys/power/state /sys/power/disk
for d in /sys/class/power_supply/*; do
    [ -e "$d" ] || continue
    readfile "$d/type" "$d/status" "$d/capacity" "$d/technology" "$d/voltage_now"
done
section 'Kernel configuration if exposed'
if [ -r /proc/config.gz ]; then
    zcat /proc/config.gz
else
    echo '/proc/config.gz unavailable'
fi
section 'Boot and recovery tools: paths only, never executed'
for tool in idme fastboot kexec fw_printenv fw_setenv flash_erase nandwrite mmc reboot; do
    command -v "$tool" 2>/dev/null || :
done
section 'Boot assets and firmware inventory (names and sizes, no contents)'
for d in /boot /lib/firmware /lib/modules /etc/kexec; do
    [ -d "$d" ] || continue
    find "$d" -maxdepth 3 -type f -exec ls -ln '{}' \;
done
section 'Boot configuration candidate paths'
for d in /etc/upstart /etc/init /etc/init.d; do
    [ -d "$d" ] || continue
    find "$d" -maxdepth 2 -type f -exec grep -l -E 'diags|bootmode|u-boot|fastboot|kexec|mmcblk|waveform|epdc' '{}' \;
done
section 'Partition table bytes (MBR entries and signature only)'
# Reading only: no output file is ever a block device.
if [ -r /dev/mmcblk0 ]; then
    dd if=/dev/mmcblk0 bs=1 skip=446 count=66 2>/dev/null | od -An -tx1
fi
section 'Bootloader version strings from bounded raw reads'
# Do not export raw boot areas: they may contain device-specific identifiers.
if command -v strings >/dev/null 2>&1; then
    for dev in /dev/mmcblk0 /dev/mmcblk0boot0 /dev/mmcblk0boot1; do
        [ -r "$dev" ] || continue
        echo "$dev: first 1 MiB, version matches only"
        dd if="$dev" bs=1024 count=1024 2>/dev/null | strings | grep -o -E 'U-Boot [0-9][0-9A-Za-z.+_-]*|U-Boot [0-9]{4}\.[0-9]{2}|Barebox [0-9][0-9A-Za-z.+_-]*' | sort -u
    done
fi
section 'Device tree identification if present'
for f in /proc/device-tree/model /proc/device-tree/compatible /sys/firmware/devicetree/base/model /sys/firmware/devicetree/base/compatible; do
    [ -r "$f" ] || continue
    echo "$f"
    tr '\000' '\n' < "$f"
done
section 'Runtime and dynamic linker files'
ls -ln /bin/busybox /sbin/init /sbin/init.exe /lib/ld* /lib/libc* /lib/libstdc++* /usr/lib/libstdc++* 2>/dev/null
readlink /proc/1/exe
readfile /proc/version /proc/sys/kernel/tainted
section 'Init jobs and process executable paths'
if command -v initctl >/dev/null 2>&1; then initctl list; fi
for proc in /proc/[0-9]*; do
    [ -r "$proc/comm" ] || continue
    printf '%s ' "${proc##*/}"
    cat "$proc/comm"
    readlink "$proc/exe" 2>/dev/null || :
done
section 'Startup, module and firmware hashes'
for d in /etc/upstart /etc/init /etc/init.d /lib/modules /lib/firmware /boot; do
    [ -d "$d" ] || continue
    if command -v sha256sum >/dev/null 2>&1; then
        find "$d" -maxdepth 3 -type f -exec sha256sum '{}' \;
    else
        echo 'sha256sum unavailable'
    fi
done
section 'Network driver bindings (no addresses or credentials)'
for d in /sys/class/net/*; do
    [ -e "$d" ] || continue
    echo "${d##*/}"
    readlink "$d/device/driver" 2>/dev/null || :
    readfile "$d/device/modalias"
done
section 'CPU frequency and idle capabilities'
for d in /sys/devices/system/cpu/cpu0/cpufreq /sys/devices/system/cpu/cpu0/cpuidle/state*; do
    [ -d "$d" ] || continue
    readfile "$d/scaling_available_frequencies" "$d/scaling_available_governors" "$d/cpuinfo_min_freq" "$d/cpuinfo_max_freq" "$d/name" "$d/desc"
done
section 'Recovery and boot-related device nodes (metadata only)'
ls -ln /dev/mmcblk* /dev/mtd* /dev/ttymxc* /dev/fb* /dev/input/event* 2>/dev/null
section 'Boot configuration references (selected operational lines only)'
for f in /etc/fstab /etc/fw_env.config /etc/inittab /etc/modules /etc/modprobe.conf; do
    [ -r "$f" ] || continue
    echo "$f"
    # Exclude comments and possible credentials; preserve device/module mappings.
    grep -v -E '^[[:space:]]*(#|$)|password|passwd|secret|token|credential|serial' "$f"
done
section 'Finished' 
echo 'No flashing, remount, service change, reboot, boot-environment mutation or hardware-register access performed.'
echo 'Tool presence does not prove recovery works or the bootloader is unlocked.'
