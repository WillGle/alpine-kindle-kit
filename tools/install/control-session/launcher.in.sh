#!/bin/sh
# Name: GleWorks Alpine Session
# Author: GleWorks
# Temporary control and returning diagnostics; no PID1 replacement or boot write.
set -eu
if [ "$(id -u)" != 0 ]; then
    exec /var/local/kmc/bin/su -c '/bin/sh /mnt/us/documents/gleworks-alpine-session.sh'
fi
umask 077
base=/mnt/us/documents/gleworks-alpine-session
[ "$(uname -r)" = 3.0.35-lab126 ] || exit 1
[ -d "$base" ] && [ ! -L "$base" ] || exit 1
[ -f "$base/SHA256SUMS" ] && [ ! -L "$base/SHA256SUMS" ] || exit 1
[ "$(sha256sum "$base/SHA256SUMS" | awk '{print $1}')" = '@MANIFEST_HASH@' ] || exit 1
mkdir /tmp/gleworks-alpine-session.lock || exit 1
work=
handed_off=0
cleanup() {
    result=$?
    trap - EXIT HUP INT TERM
    if [ "$handed_off" = 0 ]; then
        [ -z "$work" ] || rm -rf "$work"
        rmdir /tmp/gleworks-alpine-session.lock || true
    fi
    exit "$result"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
work=$(mktemp -d /tmp/gleworks-alpine-session.XXXXXX)
chmod 700 "$work"
cd "$base"
sha256sum -c SHA256SUMS > "$work/input-check.txt"
for file in dropbear libz.so.1 server-key authorized_keys busybox ld-musl-armhf.so.1 alpine-release session.sh gleworks-alpine-stage.sh; do
    [ -f "$file" ] && [ ! -L "$file" ] || exit 1
    cp "$file" "$work/$file"
done
cd "$work"
sha256sum -c "$base/SHA256SUMS" > copied-input-check.txt
chmod 700 dropbear session.sh
chmod 600 server-key authorized_keys
out=/mnt/us/gleworks-alpine-session-$(date +%Y%m%d-%H%M%S)-$$
mkdir "$out"
cp input-check.txt copied-input-check.txt "$out/"
# From here the worker/script/binaries are in tmpfs, independent of USB storage.
nohup /bin/sh "$work/session.sh" "$work" "$out" '@HOST@' > "$work/session.log" 2>&1 < /dev/null &
worker=$!
handed_off=1
while [ ! -f "$work/ready.txt" ]; do
    if ! kill -0 "$worker" 2>/dev/null || [ -f "$work/failed.txt" ]; then
        cp "$work/session.log" "$out/session.log" 2>/dev/null || true
        exit 1
    fi
    sleep 1
done
printf '%s\n' 'Temporary Alpine diagnostic/control session started. No native boot or OS deletion.'
printf '%s\n' "Evidence: $out"
