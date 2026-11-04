#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Leave the serial recovery shell available if early boot failed or was bypassed.
set -eu
if [ -f /run/core-boot-ready ] && [ ! -f /run/core-recovery ]; then
    exec /usr/sbin/gw-system --supervise
fi
echo GW_CORE_RECOVERY_CONSOLE
exec /bin/sleep 2147483647
