#!/bin/sh
# Give the library time to settle, then keep feedback visible long enough to read.
set -eu
umask 077
hold=${1:-10}
return_to=${2:-home}
case "$return_to" in redraw|home) ;; *) exit 1 ;; esac
case "$hold" in ''|*[!0-9]*) exit 1 ;; esac
[ "$hold" -ge 1 ] && [ "$hold" -le 300 ] || exit 1
action=${3:-notification}
case "$action" in ''|*[!A-Za-z0-9_.-]*) exit 1 ;; esac
status=${4:-0}
case "$status" in ''|*[!0-9]*) exit 1 ;; esac
[ "$status" -le 255 ] || exit 1
kind=INFO
[ "$status" = 0 ] || kind=ERROR
base=${0%/scripts/*}
recorder=$base/scripts/record-failure.sh
runtime=/tmp/gleworks
[ ! -L "$runtime" ] || exit 1
[ -d "$runtime" ] || mkdir "$runtime"
[ "$(stat -c %u "$runtime")" = 0 ] || exit 1
chmod 700 "$runtime"
log_failure() { [ ! -f "$recorder" ] || /bin/sh "$recorder" "$action" "$1" "$2" || :; }
popup=$runtime/popup-$$
# Direct framebuffer output must hand the screen back to the Amazon UI.
restore_screen() {
    result=$?
    trap - EXIT
    set +e
    [ "$result" = 0 ] || log_failure "$result" display
    if [ -d "$popup" ] && [ ! -L "$popup" ]; then rm -rf "$popup";fi
    if [ "$return_to" = home ] && [ -x /usr/bin/lipc-set-prop ]; then
        if /usr/bin/lipc-set-prop com.lab126.appmgrd start app://com.lab126.booklet.home >"$runtime/return-home.log" 2>&1; then
            printf '%s\n' 'Home: requested' >>"$runtime/display-result"
        else log_failure 1 home;printf '%s\n' 'Home: failed' >>"$runtime/display-result";fi
    fi
    if [ -x /usr/bin/xrefresh ]; then
        if /usr/bin/xrefresh -d :0.0 >"$runtime/redraw.log" 2>&1; then
            printf '%s\n' 'Redraw: requested' >>"$runtime/display-result"
        else log_failure 1 redraw;printf '%s\n' 'Redraw: failed' >>"$runtime/display-result";fi
    fi
    exit "$result"
}
trap restore_screen EXIT
trap 'exit 1' HUP INT TERM
printf '%s\n' 'Display: not rendered' >"$runtime/display-result"
mkdir "$popup"
pages=$(cd /mnt/us/koreader && ./luajit -e 'require("setupkoenv")' "$base/device/popup.lua" "$popup" "GleWorks $kind | $action")
case "$pages" in ''|*[!0-9]*) exit 1 ;; esac
[ "$pages" -ge 1 ] && [ "$pages" -le 32 ] || exit 1
sleep 1
page=1
while [ "$page" -le "$pages" ]; do
    panel=$(cat "$popup/$page")
    rendered=no
    for renderer in /var/local/kmc/bin/fbink /var/local/kmc/kindlepw2/bin/fbink /mnt/us/koreader/fbink; do
        [ -x "$renderer" ] || continue
        # Equal-width lines form one white panel; never clear the full screen.
        if "$renderer" -q -m -M -y -10 -S 2 "$panel" >"$runtime/display.log" 2>&1; then
            rendered=yes
            printf '%s\n' 'fbink: success' >"$runtime/display-result"
            break
        fi
        log_failure 1 renderer
    done
    if [ "$rendered" = no ]; then
        printf '%s\n' "$panel" | {
            row=6
            while IFS= read -r line; do
                eips 2 "$row" "$line" || exit 1
                row=$((row+1))
            done
        }
        printf '%s\n' 'eips: success' >"$runtime/display-result"
    fi
    if [ "$page" -lt "$pages" ]; then sleep 8;else sleep "$hold";fi
    page=$((page+1))
done
