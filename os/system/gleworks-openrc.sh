#!/bin/sh
# BusyBox starts this after early mounts and USB setup, alongside the console.
set -eu
[ -f /run/usb-ready ] && [ -f /run/persistent-boot ]
/sbin/openrc sysinit
/sbin/openrc boot
exec /sbin/openrc default
