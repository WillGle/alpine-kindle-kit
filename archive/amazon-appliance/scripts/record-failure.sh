#!/bin/sh
# Structured metadata only; raw command output must stay in private runtime logs.
set -eu
umask 077
action=${1:-unknown}
code=${2:-1}
category=${3:-action}
case "$action" in *[!A-Za-z0-9_.-]*) exit 1 ;; esac
case "$category" in *[!A-Za-z0-9_.-]*) exit 1 ;; esac
[ -n "$action" ] && [ "${#action}" -le 64 ] && [ -n "$category" ] && [ "${#category}" -le 64 ] || exit 1
case "$code" in ''|*[!0-9]*) exit 1 ;; esac
[ "$code" -gt 0 ] && [ "$code" -le 255 ] || exit 1
logs=/mnt/us/gleworks-logs
runtime=/tmp/gleworks
for directory in "$runtime" "$logs"; do
    [ ! -L "$directory" ] || exit 1
    [ -d "$directory" ] || mkdir "$directory"
done
[ "$(stat -c %u "$runtime")" = 0 ] || exit 1
chmod 700 "$runtime"
locked=no
for attempt in 1 2 3; do
    if mkdir "$runtime/log.lock" 2>/dev/null; then locked=yes;break;fi
    sleep 1
done
[ "$locked" = yes ] || exit 1
trap 'rmdir "$runtime/log.lock"' EXIT
trap 'exit 1' HUP INT TERM
for file in "$logs/errors.log" "$logs/errors.log.1"; do
    [ ! -L "$file" ] && { [ ! -e "$file" ] || [ -f "$file" ]; } || exit 1
done
if [ -f "$logs/errors.log" ] && [ "$(stat -c %s "$logs/errors.log")" -ge 262144 ]; then
    mv -f "$logs/errors.log" "$logs/errors.log.1"
fi
printf '%s action=%s exit=%s category=%s\n' "$(date -u '+%Y-%m-%dT%H:%M:%SZ')" "$action" "$code" "$category" >>"$logs/errors.log"
