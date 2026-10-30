#!/bin/sh
# Name: GleWorks Update Apps
# Author: GleWorks
set -eu
if [ "$(id -u)" != 0 ]; then
    exec /var/local/kmc/bin/su -c '/bin/sh /mnt/us/documents/gleworks-update.sh'
fi
umask 077
finish_app() {
    code=$?
    trap - EXIT
    recorder=/mnt/us/gleworks/scripts/record-failure.sh
    if [ ! -f "$recorder" ] && [ -n "${helper:-}" ]; then recorder=${helper%/*}/record-failure.sh;fi
    if [ "$code" != 0 ] && [ -f "$recorder" ]; then
        /bin/sh "$recorder" update "$code" launcher || :
    fi
    exit "$code"
}
trap finish_app EXIT
trap 'exit 1' HUP INT TERM
logdir=/mnt/us/gleworks-logs
[ ! -L "$logdir" ] || exit 1
[ -d "$logdir" ] || mkdir "$logdir"
log=$logdir/app-update-$(date +%Y%m%d-%H%M%S)-$$.log
set -C
result=0
if /bin/sh /mnt/us/gleworks/scripts/kindle-services.sh update-apps >"$log" 2>&1; then
    message='App operation completed. Open Status for the shared report.'
else result=$?;message="App operation stopped. Inspect $log before retrying.";fi
printf '%s\n' "$message" | /bin/sh /mnt/us/gleworks/scripts/show-message.sh 30 home update "$result"
exit "$result"
