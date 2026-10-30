#!/bin/sh
# Manual sessions only: no boot hooks, firewall openings, or keep-awake changes.
set -eu
base=${GLE_KINDLE_HOME:-/mnt/us/gleworks}
private=${GLE_KINDLE_PRIVATE:-/var/local/gleworks}
reader=${GLE_KINDLE_READER:-/mnt/us/koreader}
runtime=/tmp/gleworks
sock=$runtime/tailscaled.sock
export GLE_KINDLE_HOME="$base" GLE_KINDLE_PRIVATE="$private"
export TS_NO_LOGS_NO_SUPPORT=true

die() { echo "$*" >&2; exit 1; }
usage() { echo 'Usage: kindle-services.sh init|preflight|status|http-start|http-stop|http-toggle|http-key|vpn-start|vpn-login|vpn-status|vpn-stop|vpn-toggle|reader|reader-isolated|reader-profile|planner|terminal|update-apps|restore-apps'; }
case ${1:-help} in
    help) usage; exit 0 ;;
    init|preflight|status|http-start|http-stop|http-toggle|http-key|vpn-start|vpn-login|vpn-status|vpn-stop|vpn-toggle|reader|reader-isolated|reader-profile|planner|terminal|update-apps|restore-apps) ;;
    *) usage >&2; exit 1 ;;
esac
[ "$(id -u)" = 0 ] || die 'Run on the Kindle as root.'
umask 077
[ ! -L "$runtime" ] || die 'Unsafe runtime directory.'
[ -d "$runtime" ] || mkdir "$runtime"
[ "$(stat -c %u "$runtime")" = 0 ] || die 'Runtime directory is not owned by root.'
chmod 700 "$runtime"
mkdir "$runtime/control.lock" 2>/dev/null || die 'Another control action is running. If interrupted, reboot before retrying.'
locked=yes
finish_control() {
    code=$?
    trap - EXIT
    [ "$locked" != yes ] || rmdir "$runtime/control.lock" || :
    if [ "$code" != 0 ] && [ -f "$base/scripts/record-failure.sh" ]; then
        /bin/sh "$base/scripts/record-failure.sh" "${1:-control}" "$code" control || :
    fi
    exit "$code"
}
action=$1
trap 'finish_control "$action"' EXIT
trap 'exit 1' HUP INT TERM

lua() { (cd "$reader" && ./luajit -e 'require("setupkoenv")' "$base/device/$1" "${2:-}"); }
expected() {
    case $1 in http) echo "$reader/luajit" ;; tailscaled) echo "$base/bin/tailscaled" ;; esac
}
running() {
    service=$1
    [ -f "$runtime/$service.pid" ] || return 1
    pid=$(cat "$runtime/$service.pid")
    case $pid in ''|*[!0-9]*) return 1 ;; esac
    [ "$pid" -gt 1 ] || return 1
    kill -0 "$pid" 2>/dev/null || return 1
    [ "$(readlink "/proc/$pid/exe" 2>/dev/null)" = "$(expected "$service")" ] || return 1
    if [ "$service" = http ]; then
        tr '\000' '\n' <"/proc/$pid/cmdline" | grep -Fx "$base/device/server.lua" >/dev/null || return 1
    fi
}
start_http() {
    if running http; then echo 'Transfer server already running'; return; fi
    lua server.lua --check
    (cd "$reader" && exec ./luajit -e 'require("setupkoenv")' "$base/device/server.lua") >"$runtime/http.log" 2>&1 &
    echo $! >"$runtime/http.pid"
    sleep 1
    running http || die "Transfer server failed; see $runtime/http.log"
    echo 'Transfer server started. Connect through Tailscale on port 8080.'
    echo 'Open GleWorks Transfer Key to display the access token.'
}
stop_service() {
    service=$1
    if running "$service"; then
        kill "$pid"
        i=0
        while kill -0 "$pid" 2>/dev/null && [ "$i" -lt 10 ]; do sleep 1; i=$((i+1)); done
        kill -0 "$pid" 2>/dev/null && die 'Service has not exited; do not reconnect USB.'
        rm -f "$runtime/$service.pid"
        echo "$service stopped"
    else echo "$service is not running with the expected executable"; fi
}
start_vpn() {
    if running tailscaled; then echo 'VPN already running'; return; fi
    [ -d "$private/tailscale" ] || die 'Run init first.'
    [ ! -S /tmp/ku-tailscaled.sock ] || die 'Old VPN socket exists; stop the old VPN first.'
    "$base/bin/tailscaled" --tun=userspace-networking --statedir="$private/tailscale" --socket="$sock" >"$runtime/tailscaled.log" 2>&1 &
    echo $! >"$runtime/tailscaled.pid"
    sleep 2
    running tailscaled || die "VPN failed; see $runtime/tailscaled.log"
    # 'down' persists WantRunning=false; each manual start must bring it up again.
    if "$base/bin/tailscale" --socket="$sock" up --timeout=5s --hostname=gleworks-kindle --accept-routes=false --accept-dns=false --ssh=false >"$runtime/login.log" 2>&1; then
        echo 'VPN connected:'
        "$base/bin/tailscale" --socket="$sock" ip -4
    else
        echo 'VPN daemon started. Open GleWorks VPN Login to enroll or inspect connectivity.'
    fi
}
stop_vpn() {
    if running tailscaled; then
        "$base/bin/tailscale" --socket="$sock" down
        stop_service tailscaled
    else echo 'VPN is not running with the expected executable'; fi
}
case $1 in
    init)
        [ ! -L "$private" ] || die 'Unsafe private directory.'
        mkdir -p "$private" "$private/tailscale" "$base/files" /mnt/us/documents/GleWorks
        [ "$(stat -c %u "$private")" = 0 ] || die 'Private directory is not owned by root.'
        chmod 700 "$private" "$private/tailscale"
        [ -e "$private/http.token" ] || lua server.lua --init-token
        ;;
    preflight)
        [ "$(uname -m)" = armv7l ] || die 'Expected Paperwhite 3 ARMv7.'
        [ -x "$reader/luajit" ] && [ -x "$reader/reader.lua" ] && [ -x "$reader/koreader.sh" ] || die 'Missing executable KOReader entrypoints.'
        [ -x "$base/bin/tailscale" ] && [ -x "$base/bin/tailscaled" ] || die 'Missing verified VPN binaries.'
        lua server.lua --check
        "$base/bin/tailscale" version
        "$base/bin/tailscaled" --version
        echo 'Preflight passed. Live VPN, HTTP, reader and sleep/wake acceptance still required.'
        ;;
    status)
        date -u
        for service in http tailscaled; do
            if running "$service"; then echo "$service running"; else echo "$service stopped"; fi
        done
        if running tailscaled; then
            if "$base/bin/tailscale" --socket="$sock" status --json >"$runtime/status.json" 2>"$runtime/status-error.log"; then
                lua vpn-state.lua "$runtime/status.json" || {
                    /bin/sh "$base/scripts/record-failure.sh" status 1 vpn_decode || :
                    echo 'VPN status could not be decoded.'
                }
            else
                /bin/sh "$base/scripts/record-failure.sh" status 1 vpn_query || :
                echo 'VPN daemon is running, but status could not be queried.'
            fi
        fi
        lua vpn-state.lua --diagnostic || echo 'VPN diagnostics unavailable.'
        http_state=stopped
        if running http; then http_state=running; fi
        lua appliance-health.lua "$http_state" 2>"$runtime/health-error.log" || {
            /bin/sh "$base/scripts/record-failure.sh" status 1 health || :
            echo 'Additional health checks failed; inspect private health-error.log.'
        }
        df -k /mnt/us /var/local
        ;;
    http-start) start_http ;;
    http-stop) stop_service http ;;
    http-toggle) if running http; then stop_service http; else start_http; fi ;;
    http-key)
        # Deliberate display action only; the scriptlet must not redirect this to a USB log.
        cat "$private/http.token"
        ;;
    vpn-start) start_vpn ;;
    vpn-login)
        running tailscaled || die 'Start VPN first.'
        if "$base/bin/tailscale" --socket="$sock" up --timeout=15s --hostname=gleworks-kindle --accept-routes=false --accept-dns=false --ssh=false >"$runtime/login.log" 2>&1; then
            echo 'VPN connected:'
            "$base/bin/tailscale" --socket="$sock" ip -4
        elif grep -q 'https://login.tailscale.com/' "$runtime/login.log"; then
            echo 'Open this login link on your computer or phone, then check VPN status:'
            grep -o 'https://login.tailscale.com/[^[:space:]]*' "$runtime/login.log" | head -n 1
        else
            echo 'VPN enrollment is not confirmed.'
            lua vpn-state.lua --diagnostic || :
            exit 1
        fi
        ;;
    vpn-status) "$base/bin/tailscale" --socket="$sock" status ;;
    vpn-stop) stop_vpn ;;
    vpn-toggle) if running tailscaled; then stop_vpn; else start_vpn; fi ;;
    update-apps|restore-apps)
        rmdir "$runtime/control.lock"; locked=no
        if [ "$1" = restore-apps ]; then exec /bin/sh "$base/scripts/apply-bundle.sh" --restore;fi
        exec /bin/sh "$base/scripts/apply-bundle.sh"
        ;;
    reader|reader-isolated|planner|terminal)
        [ -f "$reader/koreader.sh" ] || die 'Verified KOReader not installed.'
        # Release the control lock before handing over to the reader lifecycle.
        rmdir "$runtime/control.lock"; locked=no
        rm -f "$runtime/update-request"
        cd "$reader"
        if [ "$1" = reader-isolated ]; then
            ./koreader.sh --framework_stop
        else
            if [ "$1" = planner ] || [ "$1" = terminal ]; then export GLE_OPEN_APP="$1";fi
            ./koreader.sh
        fi
        if [ -f "$runtime/update-request" ] && [ ! -L "$runtime/update-request" ]; then
            rm "$runtime/update-request"
            exec /bin/sh /mnt/us/documents/gleworks-update.sh
        fi
        ;;
    reader-profile)
        # KOReader writes settings on exit, so never edit them while it is running.
        for proc in /proc/[0-9]*/cmdline; do
            [ -r "$proc" ] || continue
            if tr '\000' '\n' <"$proc" 2>/dev/null | grep -E '(^|/)reader.lua$' >/dev/null; then
                die 'Close KOReader before applying the profile.'
            fi
        done
        lua reader-profile.lua
        ;;
esac
