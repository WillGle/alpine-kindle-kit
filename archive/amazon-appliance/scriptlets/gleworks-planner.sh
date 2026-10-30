#!/bin/sh
# Name: GleWorks Calendar and Todo
# Author: GleWorks
set -eu
if [ "$(id -u)" != 0 ]; then
    exec /var/local/kmc/bin/su -c '/bin/sh /mnt/us/documents/gleworks-planner.sh'
fi
umask 077
finish_app() {
    code=$?
    trap - EXIT
    recorder=/mnt/us/gleworks/scripts/record-failure.sh
    if [ ! -f "$recorder" ] && [ -n "${helper:-}" ]; then recorder=${helper%/*}/record-failure.sh;fi
    if [ "$code" != 0 ] && [ -f "$recorder" ]; then
        /bin/sh "$recorder" planner "$code" launcher || :
    fi
    exit "$code"
}
trap finish_app EXIT
trap 'exit 1' HUP INT TERM
if /bin/sh /mnt/us/gleworks/scripts/kindle-services.sh planner; then
    exit 0
else
    result=$?
    printf '%s\n' 'Calendar/Todo could not start or exit cleanly. Check koreader/crash.log and close other GleWorks apps before retrying.' |
        /bin/sh /mnt/us/gleworks/scripts/show-message.sh 30 home planner "$result"
    exit "$result"
fi
