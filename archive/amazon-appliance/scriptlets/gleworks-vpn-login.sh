#!/bin/sh
# Name: GleWorks VPN Login
# Author: GleWorks
set -eu
if [ "$(id -u)" != 0 ]; then
    exec /var/local/kmc/bin/su -c '/bin/sh /mnt/us/documents/gleworks-vpn-login.sh'
fi
umask 077
finish_app() {
    code=$?
    trap - EXIT
    recorder=/mnt/us/gleworks/scripts/record-failure.sh
    if [ ! -f "$recorder" ] && [ -n "${helper:-}" ]; then recorder=${helper%/*}/record-failure.sh;fi
    if [ "$code" != 0 ] && [ -f "$recorder" ]; then
        /bin/sh "$recorder" vpn-login "$code" launcher || :
    fi
    exit "$code"
}
trap finish_app EXIT
trap 'exit 1' HUP INT TERM
# Keep tokens, login URLs and VPN output off USB logs.
result=0
hold=30
message=$(/bin/sh /mnt/us/gleworks/scripts/kindle-services.sh vpn-login 2>&1) || {
    result=$?
    hold=30
    message="Action needs attention: $message"
}
case "$message" in
    *https://login.tailscale.com/*)
        message="$message

Link stays visible for 3 minutes.
Home returns when the timer ends.
After authorizing on your phone, open GleWorks Status."
        hold=180 ;;
    *) hold=30 ;;
esac
printf '%s\n' "$message" | /bin/sh /mnt/us/gleworks/scripts/show-message.sh "$hold" home vpn-login "$result"
exit "$result"
