#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Single idempotent device-node and module preparation for the PW3.
#
# Sourced by gleworks-early.sh, gleworks-hardware-prepare.sh and
# gleworks-display.sh.  Every function tolerates already-present mounts,
# nodes and modules.  POSIX sh; no host assumptions.

bb=/bin/busybox
# Test/boot-harness seams: point these at private fixtures to exercise the
# already-loaded path without real modules.
modules_file=${GW_MODULES_FILE:-/proc/modules}
insmod_cmd=${GW_INSMOD:-"$bb insmod"}
touch_module_dir=/lib/modules/3.0.35-lab126/kernel/drivers/input/touchscreen
usb_module_dir=/lib/modules/3.0.35-lab126/kernel/drivers/usb

mounted() {
    $bb grep -q " $1 " /proc/mounts
}

mount_early_filesystems() {
    mounted /proc || $bb mount -t proc proc /proc
    mounted /sys || $bb mount -t sysfs sysfs /sys
    if ! mounted /dev; then
        $bb mount -t tmpfs -o mode=755 tmpfs /dev
    fi
    $bb mkdir -p /dev/pts /dev/shm /dev/input /var/tmp /sys/kernel/debug
    mounted /dev/pts || $bb mount -t devpts devpts /dev/pts
    mounted /dev/shm || $bb mount -t tmpfs -o mode=1777 tmpfs /dev/shm
    mounted /run || $bb mount -t tmpfs -o mode=755 tmpfs /run
    mounted /tmp || $bb mount -t tmpfs -o mode=1777 tmpfs /tmp
}

node_from_sysfs() { # sysfs dev file -> node path
    [ -f "$1" ] || return 0
    numbers=$($bb cat "$1")
    case "$numbers" in *:*) ;; *) return 0;; esac
    $bb mknod "$2" c "${numbers%:*}" "${numbers#*:}" 2>/dev/null || true
}

create_core_devices() {
    $bb mkdir -p /dev/input /var/tmp
    [ -e /dev/null ] || $bb mknod /dev/null c 1 3
    [ -e /dev/console ] || $bb mknod /dev/console c 5 1
    [ -e /dev/urandom ] || $bb mknod /dev/urandom c 1 9
    [ -e /dev/random ] || $bb mknod /dev/random c 1 8
    [ -e /dev/zero ] || $bb mknod /dev/zero c 1 5
    [ -e /dev/ptmx ] || $bb mknod /dev/ptmx c 5 2
    $bb chmod 600 /dev/console
    $bb chmod 666 /dev/null
    $bb chmod 644 /dev/urandom /dev/random
    $bb chmod 666 /dev/zero
    [ -f /sys/class/rtc/rtc0/dev ] && node_from_sysfs /sys/class/rtc/rtc0/dev /dev/rtc0
    [ -e /dev/rtc0 ] && $bb chmod 600 /dev/rtc0
    # Framebuffer node for the EPDC port.
    [ -e /dev/fb0 ] || $bb mknod /dev/fb0 c 29 0
    [ -e /dev/fb0 ] && $bb chmod 600 /dev/fb0
}

load_touch_modules() {
    for name in cyttsp4_i2c cyttsp4_core cyttsp4_mt_b; do
        if ! $bb grep -q "^$name " "$modules_file"; then
            $insmod_cmd "$touch_module_dir/$name.ko"
        fi
    done
}

create_input_nodes() {
    for input in /sys/class/input/event*; do
        [ -f "$input/dev" ] || continue
        node_from_sysfs "$input/dev" "/dev/input/${input##*/}"
    done
    for node in /dev/input/event*; do
        [ -e "$node" ] && $bb chmod 600 "$node"
    done
}

# Print the event node for the cyttsp4 touchscreen, if present.
find_touch_node() {
    for name in /sys/class/input/event*/device/name; do
        [ -f "$name" ] || continue
        case "$($bb cat "$name" 2>/dev/null)" in
            *cyttsp4*)
                event=${name#/sys/class/input/}
                event=${event%%/*}
                echo "/dev/input/$event"
                return 0
                ;;
        esac
    done
    return 1
}

load_usb_gadget() {
    for name in fsl_otg_arc arcotg_udc; do
        if ! $bb grep -q "^$name " "$modules_file"; then
            case "$name" in
                fsl_otg_arc) $insmod_cmd "$usb_module_dir/otg/$name.ko" ;;
                arcotg_udc)  $insmod_cmd "$usb_module_dir/gadget/$name.ko" ;;
            esac
        fi
    done
}
