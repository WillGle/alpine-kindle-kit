#!/bin/sh
# Name: GleWorks Terminal
# Author: GleWorks
set -eu
if [ "$(id -u)" != 0 ]; then
    exec /var/local/kmc/bin/su -c '/bin/sh /mnt/us/documents/gleworks-terminal.sh'
fi
umask 077
finish_app() {
    code=$?
    trap - EXIT
    recorder=/mnt/us/gleworks/scripts/record-failure.sh
    if [ ! -f "$recorder" ] && [ -n "${helper:-}" ]; then recorder=${helper%/*}/record-failure.sh;fi
    if [ "$code" != 0 ] && [ -f "$recorder" ]; then
        /bin/sh "$recorder" terminal "$code" launcher || :
    fi
    exit "$code"
}
trap finish_app EXIT
trap 'exit 1' HUP INT TERM
if /bin/sh /mnt/us/gleworks/scripts/kindle-services.sh terminal; then
    exit 0
else
    result=$?
    printf '%s\n' 'Terminal could not start or exit cleanly. Check koreader/crash.log and close other GleWorks apps before retrying.' |
        /bin/sh /mnt/us/gleworks/scripts/show-message.sh 30 home terminal "$result"
    exit "$result"
fi
