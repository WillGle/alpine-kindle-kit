#!/bin/sh
# Name: GleWorks Restore Apps
# Author: GleWorks
set -eu
if [ "$(id -u)" != 0 ]; then
    exec /var/local/kmc/bin/su -c '/bin/sh /mnt/us/documents/gleworks-restore-apps.sh'
fi
umask 077
finish_app() {
    code=$?
    trap - EXIT
    recorder=/mnt/us/gleworks/scripts/record-failure.sh
    if [ ! -f "$recorder" ] && [ -n "${helper:-}" ]; then recorder=${helper%/*}/record-failure.sh;fi
    if [ "$code" != 0 ] && [ -f "$recorder" ]; then
        /bin/sh "$recorder" restore-apps "$code" launcher || :
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
resolve_restore() {
    restore_script=/mnt/us/gleworks/scripts/apply-bundle.sh
    if [ ! -f "$restore_script" ]; then
        previous=$(cat /mnt/us/gleworks-last-app-update.txt) || return 1
        suffix=${previous#/mnt/us/gleworks-app-backup-}
        [ "$suffix" != "$previous" ] && [ -n "$suffix" ] || return 1
        case "$suffix" in *[!0-9-]*) return 1 ;; esac
        [ ! -L "$previous" ] && [ ! -L "$previous/gleworks" ] || return 1
        restore_script=$previous/gleworks/scripts/apply-bundle.sh
    fi
    [ -f "$restore_script" ] && [ ! -L "$restore_script" ] || return 1
    printf '%s\n' "$restore_script"
}
result=0
if restore_script=$(resolve_restore 2>/dev/null); then
    if /bin/sh "$restore_script" --restore >"$log" 2>&1; then
        message='App operation completed. Open Status for the shared report.'
    else result=$?;message="App operation stopped. Inspect $log before retrying.";fi
else result=1;message='No usable application backup found. Keep existing files and inspect the last-update record.';fi
helper=/mnt/us/gleworks/scripts/show-message.sh
if [ ! -f "$helper" ] && [ -n "${restore_script:-}" ]; then
    helper=${restore_script%/*}/show-message.sh
fi
if [ -f "$helper" ]; then
    printf '%s\n' "$message" | /bin/sh "$helper" 30 home restore-apps "$result"
else
    printf '%s\n' "$message" >&2
    /usr/bin/lipc-set-prop com.lab126.appmgrd start app://com.lab126.booklet.home 2>/dev/null || :
    /usr/bin/xrefresh -d :0.0 2>/dev/null || :
    result=1
fi
exit "$result"
