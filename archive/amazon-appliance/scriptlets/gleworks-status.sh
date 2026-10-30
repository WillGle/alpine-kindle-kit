#!/bin/sh
# Name: GleWorks Status
# Author: GleWorks
set -eu
if [ "$(id -u)" != 0 ]; then
    exec /var/local/kmc/bin/su -c '/bin/sh /mnt/us/documents/gleworks-status.sh'
fi
umask 077
finish_app() {
    code=$?
    trap - EXIT
    recorder=/mnt/us/gleworks/scripts/record-failure.sh
    if [ ! -f "$recorder" ] && [ -n "${helper:-}" ]; then recorder=${helper%/*}/record-failure.sh;fi
    if [ "$code" != 0 ] && [ -f "$recorder" ]; then
        /bin/sh "$recorder" status "$code" launcher || :
    fi
    exit "$code"
}
trap finish_app EXIT
trap 'exit 1' HUP INT TERM
# Keep tokens, login URLs and VPN output off USB logs.
result=0
hold=30
message=$(/bin/sh /mnt/us/gleworks/scripts/kindle-services.sh status 2>&1) || {
    result=$?
    hold=30
    message="Action needs attention: $message"
}
# Status contains service state, IP and storage only; no enrollment URL or token.
report_ok=yes
if ! printf '%s\n' "$message" > /mnt/us/documents/GleWorks-status.txt; then
    report_ok=no
    result=1
    message="$message
Status report could not be saved to USB storage."
fi
if ! printf '%s\n' "$message" | /bin/sh /mnt/us/gleworks/scripts/show-message.sh "$hold" home status "$result"; then
    result=1
    if [ "$report_ok" = yes ]; then printf '%s\n' 'Display action failed; service status above is still available.' >> /mnt/us/documents/GleWorks-status.txt;fi
fi
if [ "$report_ok" = yes ] && [ -r /tmp/gleworks/display-result ]; then
    cat /tmp/gleworks/display-result >> /mnt/us/documents/GleWorks-status.txt
fi
exit "$result"
