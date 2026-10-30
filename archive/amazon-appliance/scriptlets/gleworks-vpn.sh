#!/bin/sh
# Name: GleWorks VPN
# Author: GleWorks
set -eu
if [ "$(id -u)" != 0 ]; then
    exec /var/local/kmc/bin/su -c '/bin/sh /mnt/us/documents/gleworks-vpn.sh'
fi
umask 077
finish_app() {
    code=$?
    trap - EXIT
    recorder=/mnt/us/gleworks/scripts/record-failure.sh
    if [ ! -f "$recorder" ] && [ -n "${helper:-}" ]; then recorder=${helper%/*}/record-failure.sh;fi
    if [ "$code" != 0 ] && [ -f "$recorder" ]; then
        /bin/sh "$recorder" vpn "$code" launcher || :
    fi
    exit "$code"
}
trap finish_app EXIT
trap 'exit 1' HUP INT TERM
# Keep tokens, login URLs and VPN output off USB logs.
result=0
hold=10
message=$(/bin/sh /mnt/us/gleworks/scripts/kindle-services.sh vpn-toggle 2>&1) || {
    result=$?
    hold=30
    message="Action needs attention: $message"
}
printf '%s\n' "$message" | /bin/sh /mnt/us/gleworks/scripts/show-message.sh "$hold" home vpn "$result"
exit "$result"
